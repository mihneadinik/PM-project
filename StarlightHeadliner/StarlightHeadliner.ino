/*************************************************************************************************\
 *                                     Files and libraries                                       *
\*************************************************************************************************/

#include "ConstantsAndTypes.h"
#include "MusicMode.hpp"
#include "LightMode.hpp"
#include "ISRsTimersADC.hpp"
#include "adaptedTinyIRReceiver.hpp"
#include <Adafruit_NeoPixel.h>

/*************************************************************************************************\
 *                                       Board pins used                                         *
\*************************************************************************************************/

// Arduino pins
#define IR_PIN 2
#define WIDE_PIN 6
#define NARROW_PIN 7
#define REVERSE_TRIGGER_PIN 3
#define SENSORS_TRIGGER_PIN 4

/*************************************************************************************************\
 *                                      Global Variables                                         *
\*************************************************************************************************/

// Neopixels init
Adafruit_NeoPixel pixelsWide(NUM_PIXELS, WIDE_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixelsNarrow(NUM_PIXELS, NARROW_PIN, NEO_GRB + NEO_KHZ800);

// Neopixels values
volatile stripParams_t wideStripParams;
volatile stripParams_t narrowStripParams;

// Program values
volatile twinkleParams_t twinkleParams;
volatile sensorsParams_t sensorParams;
lightMode_t lightMode;
volatile uint8_t command; // Received from the remote
volatile bool brightnessChanged; // Flag set after ADC conversion to update LEDs brightness

/*************************************************************************************************\
 *                                     Function prototypes                                       *
\*************************************************************************************************/

void set_initial_values();
void decode_command();
void execute_mode();
void _changeSensorsPower();
void handle_sensors();

/*************************************************************************************************\
 *                                      Arduino functions                                        *
\*************************************************************************************************/

void setup() {
  Serial.begin(9600);
  // Initial setups
  setup_receiver_and_interrupts();
  setup_timer1();
  setup_timer2();
  setup_reverse_interrupts();
  setup_ADC();
  set_initial_values();

  // Neopixels startup
  pixelsWide.begin();
  pixelsNarrow.begin();
}

void loop() {
  // Flag set from interrupt when new command is received
  if (lightMode.modeChange) {
    // Clear flag
    lightMode.modeChange = false;
    // Handle new command from interrupt
    decode_command();
  }

  if (sensorParams.signalPower) {
    // Clear flag
    sensorParams.signalPower = false;
    handle_sensors();
  }

  // Update LEDs based on selected light mode
  execute_mode();
}

/*************************************************************************************************\
 *                                    Initial values setups                                      *
\*************************************************************************************************/

void set_initial_values() {
  // Neopixel params
  wideStripParams.hue = HUE_RED;
  wideStripParams.saturation = SATURATION_COLOR;
  wideStripParams.brightness = MAX_BRIGHTNESS;
  narrowStripParams.hue = HUE_RED;
  narrowStripParams.saturation = SATURATION_COLOR;
  narrowStripParams.brightness = MAX_BRIGHTNESS;
  wideStripParams.selected = true;
  narrowStripParams.selected = true;
  wideStripParams.rainbow = false;
  narrowStripParams.rainbow = false;
  wideStripParams.twinkle = false;
  narrowStripParams.twinkle = false;

  // Sensors params
  sensorParams.currOverflows = 0;
  sensorParams.poweredOn = false;
  sensorParams.signalPower = false;

  // Program params (starts on white-red twinkle)
  command = IR_7;
  lightMode.currMode = TWINKLE;
  lightMode.prevMode = TWINKLE;
  twinkleParams.twinkleLEDOffset = 0;
  twinkleParams.twinkleChange = false;
  lightMode.modeChange = true; // Force set flag to execute default command
  brightnessChanged = false;
}

/*************************************************************************************************\
 *                                       Driver functions                                        *
\*************************************************************************************************/

// Applies changes to the LEDs
void execute_mode() {
  switch (lightMode.currMode) {
    case STATIC:
      static_mode();
      // Change mode to blank state after applying changes
      lightMode.currMode = NOTHING;
      break;
    
    case TWINKLE:
      twinkle_mode();
      break;

    case MUSIC:
      // Flag set at the end of ADC conversion
      if (brightnessChanged) {
        // Update brightness and clear flag
        brightnessChanged = false;
        static_mode();
      }
      break;
    
    case NOTHING:
      // No need to update anything
      break;
  }
}

// Decodes the code received from remote
void decode_command() {
  // Save last state
  lightMode.prevMode = lightMode.currMode;

  switch (command) {
    // Both static white color mode
    case IR_1:
      // Set white color (only need max saturation)
      wideStripParams.saturation = SATURATION_WHITE;
      narrowStripParams.saturation = SATURATION_WHITE;

      // Clear rainbow and twinkle flags
      wideStripParams.twinkle = false;
      narrowStripParams.twinkle = false;
      wideStripParams.rainbow = false;
      narrowStripParams.rainbow = false;

      // Update current light mode
      lightMode.currMode = STATIC;
      break;
    
    // Both static red color mode
    case IR_2:
      // Set color specific saturation value
      wideStripParams.saturation = SATURATION_COLOR;
      narrowStripParams.saturation = SATURATION_COLOR;
      // Set red color 
      wideStripParams.hue = HUE_RED;
      narrowStripParams.hue = HUE_RED;

      // Clear rainbow and twinkle flags
      wideStripParams.twinkle = false;
      narrowStripParams.twinkle = false;
      wideStripParams.rainbow = false;
      narrowStripParams.rainbow = false;

      // Update current light mode
      lightMode.currMode = STATIC;
      break;

    // Both static random color mode
    case IR_3:
      // Set color specific saturation value
      wideStripParams.saturation = SATURATION_COLOR;
      narrowStripParams.saturation = SATURATION_COLOR;
      // Choose a random color
      wideStripParams.hue = narrowStripParams.hue = get_random_color();

      // Clear rainbow and twinkle flags
      wideStripParams.twinkle = false;
      narrowStripParams.twinkle = false;
      wideStripParams.rainbow = false;
      narrowStripParams.rainbow = false;

      // Update current light mode
      lightMode.currMode = STATIC;
      break;
    
    // Both twinkle white color mode
    case IR_4:
      // Set white color (only need max saturation)
      wideStripParams.saturation = SATURATION_WHITE;
      narrowStripParams.saturation = SATURATION_WHITE;

      // Set twinkle flag (both strips) and clear rainbow flag
      wideStripParams.twinkle = true;
      narrowStripParams.twinkle = true;
      wideStripParams.rainbow = false;
      narrowStripParams.rainbow = false;

      // update current light mode
      lightMode.currMode = TWINKLE;
      break;
    
    // Both twinkle red color mode
    case IR_5:
      // Set color specific saturation value
      wideStripParams.saturation = SATURATION_COLOR;
      narrowStripParams.saturation = SATURATION_COLOR;
      // Set red color
      wideStripParams.hue = HUE_RED;
      narrowStripParams.hue = HUE_RED;
      
      // Set twinkle flag (both strips) and clear rainbow flag
      wideStripParams.twinkle = true;
      narrowStripParams.twinkle = true;
      wideStripParams.rainbow = false;
      narrowStripParams.rainbow = false;

      // update current light mode
      lightMode.currMode = TWINKLE;
      break;

    // Both twinkle rainbow mode
    case IR_6:
      // Set color specific saturation value
      wideStripParams.saturation = SATURATION_COLOR;
      narrowStripParams.saturation = SATURATION_COLOR;

      // Set twinkle (both strips) and rainbow flags
      wideStripParams.twinkle = true;
      narrowStripParams.twinkle = true;
      wideStripParams.rainbow = true;
      narrowStripParams.rainbow = true;

      // Update current light mode
      lightMode.currMode = TWINKLE;
      break;

    // Single (narrow) white twinkle with static red color mode
    case IR_7:
      // Set colors
      wideStripParams.saturation = SATURATION_COLOR;
      narrowStripParams.saturation = SATURATION_WHITE;
      wideStripParams.hue = HUE_RED;

      // Set twinkle (one strip) flag and clear rainbow flag
      wideStripParams.twinkle = false;
      narrowStripParams.twinkle = true;
      wideStripParams.rainbow = false;
      narrowStripParams.rainbow = false;

      // Update current light mode
      lightMode.currMode = TWINKLE;
      break;
    
    // Single (narrow) white twinkle with static random color mode
    case IR_8:
      // Set colors
      wideStripParams.saturation = SATURATION_COLOR;
      narrowStripParams.saturation = SATURATION_WHITE;
      wideStripParams.hue = get_random_color();

      // Set twinkle (one strip) flag and clear rainbow flag
      wideStripParams.twinkle = false;
      narrowStripParams.twinkle = true;
      wideStripParams.rainbow = false;
      narrowStripParams.rainbow = false;

      // Update current light mode
      lightMode.currMode = TWINKLE;
      break;

    // Single (narrow) white twinkle with rainbow color mode
    case IR_9:
      // Set colors
      wideStripParams.saturation = SATURATION_COLOR;
      narrowStripParams.saturation = SATURATION_WHITE;

      // Set twinkle (one strip) flag and set rainbow flag
      wideStripParams.twinkle = false;
      narrowStripParams.twinkle = true;
      wideStripParams.rainbow = true;
      narrowStripParams.rainbow = false;

      // update current light mode
      lightMode.currMode = TWINKLE;
      break;

    // Select wide only (for brightness or colour change)
    case IR_STAR:
      wideStripParams.selected = true;
      narrowStripParams.selected = false;
      break;
    
    // Select both (for brightness or colour change)
    case IR_0:
      wideStripParams.selected = true;
      narrowStripParams.selected = true;
      break;

    // Select narrow only (for brightness or colour change)
    case IR_HASHTAG:
      narrowStripParams.selected = true;
      wideStripParams.selected = false;
      break;

    // Increase brightness on selection
    case IR_UP:
      change_brightness(INCREASE);
      break;
    
    // Change color counter-clockwise on selection
    case IR_LEFT:
      change_color(DECREASE);
      break;

    // Music reactive mode (will work on current colours)
    case IR_OK:
      // Update current light mode
      lightMode.currMode = MUSIC;
      break;

    // Change color clockwise on selection
    case IR_RIGHT:
      change_color(INCREASE);
      break;
    
    // Decrease brightness on selection
    case IR_DOWN:
      change_brightness(DECREASE);
      break;
  }

  // Timer and ADC checks for current mode configuration
  update_timer_status();
  update_ADC_status();
}

// Signals front sensors to turn on or off
void _changeSensorsPower() {
  // Change power state
  sensorParams.poweredOn = !sensorParams.poweredOn;

  // Send power impulse
  pinMode(SENSORS_TRIGGER_PIN, OUTPUT);
  digitalWrite(SENSORS_TRIGGER_PIN, LOW);
  delay(100);
  // Set pin back to high impedance
  pinMode(SENSORS_TRIGGER_PIN, INPUT);
}

// Handles interrupts on PD3
void handle_sensors() {
  // Time passed -> turn off sensors
  if (sensorParams.currOverflows >= SENSORS_OVERFLOWS) {
    if (sensorParams.poweredOn) {
      _changeSensorsPower();
    }

    sensorParams.currOverflows = 0;
    return;
  }

  // Time has not passed yet -> check if sensors should turn on
  if (!sensorParams.poweredOn) {
      _changeSensorsPower();
    }
}

// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
// https://github.com/Arduino-IRremote/Arduino-IRremote#timer-and-pin-usage
