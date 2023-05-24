/*************************************************************************************************\
 *                                     Files and libraries                                       *
\*************************************************************************************************/

#include "ConstantsAndTypes.h"
#include "adaptedTinyIRReceiver.hpp"
#include <Adafruit_NeoPixel.h>

/*************************************************************************************************\
 *                                       Board pins used                                         *
\*************************************************************************************************/

// Arduino pins
#define IR_PIN 2
#define WIDE_PIN 6
#define NARROW_PIN 7

/*************************************************************************************************\
 *                                      Global Variables                                         *
\*************************************************************************************************/

// Neopixels
Adafruit_NeoPixel pixelsWide(NUM_PIXELS, WIDE_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixelsNarrow(NUM_PIXELS, NARROW_PIN, NEO_GRB + NEO_KHZ800);

// Neopixels values
volatile stripParams_t wideStripParams;
volatile stripParams_t narrowStripParams;

// Program values
volatile twinkleParams_t twinkleParams;
lightMode_t lightMode;
uint8_t command; // Received from the remote
bool brightnessChanged; // Flag set after ADC conversion to update LEDs brightness

// Interrupt routine for twinkle effect
ISR(TIMER1_COMPA_vect) {
  twinkleParams.twinkleChange = true;
}

// Interrupt routine for music input capture
ISR(TIMER1_COMPB_vect) {
  // Start ADC conversion
  ADCSRA |= (1 << ADSC);
}

// Interrupt routine ADC
ISR(ADC_vect) {
  // Read conversion and set brightness accordingly (no less than 10 -> 4%)
  uint8_t externNoise = (ADCH > 10) ? ADCH : 10;

  uint8_t brightnessNarrow_new = externNoise;
  // Add a random brightness increase
  uint8_t brightnessWide_new = (externNoise + ((externNoise > 10) ? 10 + rand() % 30 : 0)) % MAX_BRIGHTNESS;
  
  // Only change with 3 quarters of the difference for smoothness
  if (brightnessNarrow_new > narrowStripParams.brightness) {
    narrowStripParams.brightness = (uint8_t)(narrowStripParams.brightness + (brightnessNarrow_new - narrowStripParams.brightness) * 0.75) % MAX_BRIGHTNESS;
  } else {
    narrowStripParams.brightness = (uint8_t)(narrowStripParams.brightness - (narrowStripParams.brightness - brightnessNarrow_new) * 0.75) % MAX_BRIGHTNESS;
  }

  // Only change with 3 quarters of the difference for smoothness
  if (brightnessWide_new > wideStripParams.brightness) {
    wideStripParams.brightness = (uint8_t)(wideStripParams.brightness + (brightnessWide_new - wideStripParams.brightness) * 0.75) % MAX_BRIGHTNESS;
  } else {
    wideStripParams.brightness = (uint8_t)(wideStripParams.brightness - (wideStripParams.brightness - brightnessWide_new) * 0.75) % MAX_BRIGHTNESS;
  }

  // Set flag to update brightness
  brightnessChanged = true;
}

// Callback after ISR routine on IR_PIN is over
void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags) {
  // Save received command
  command = aCommand;
  // Set flag to decode it
  lightMode.modeChange = true;
}

// Sets the ADC to Free Running mode
void setup_ADC() {
  cli();

  // Clear registers
  ADCSRB = 0;
  ADCSRA = 0;

  // Enable ADC interrupts
  ADCSRA |= (1 << ADIE);

  // Set prescaler to 128
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // Set 1.1V refference
  ADMUX |= (1 << REFS0) | (1 << REFS1);
  // Set 8 bit conversion (to match with maximum brightness)
  ADMUX |= (1 << ADLAR);

  sei();
}

void enable_ADC() {
  // Enable ADC
  ADCSRA |= (1 << ADEN);
}

void disable_ADC() {
  // Disable ADC
  ADCSRA &= ~(1 << ADEN);
}

// Sets timer1
void setup_timer1() {
  cli();

  // Clear the registers
  TCNT1 = 0;
  TCCR1A = 0;
  TCCR1B = 0;
  // Set the timer to stop on compare match
  TCCR1A = 0;
  TCCR1B |= (1 << WGM12);
  // Set prescaler to 256
  TCCR1B |= (1 << CS12);
  // Set compare values (channel A - twinkle and channel B - music)
  OCR1A = TIMER_TWINKLE_COMPARE;
  OCR1B = TIMER_MUSIC_COMPARE;
  // Activate interrupt on compare match
  TIMSK1 |= (1 << OCIE1A);

  sei();
}

// Stops timer counting channel for twkinkle changes
void stop_twinkle_timer() {
  cli();

  // Disable channel A interrupt
  TIMSK1 &= ~(1 << OCIE1A);
  
  sei();
}

// Stops timer counting channel for music changes
void stop_music_timer() {
  cli();

  // Disable channel B interrupt
  TIMSK1 &= ~(1 << OCIE1B);
  // Disable ADC
  disable_ADC();
  
  sei();
}

// Restarts the twinkle counting
void start_twinkle_timer() {
  cli();

  // Change top value
  OCR1A = TIMER_TWINKLE_COMPARE;
  // Enable channel A interrupt
  TIMSK1 |= (1 << OCIE1A);

  sei();
}

// Restarts the external sound capture counting
void start_music_timer() {
  cli();

  // Change top value
  OCR1A = TIMER_MUSIC_COMPARE;
  // Enable channel B interrupt
  TIMSK1 |= (1 << OCIE1B);
  // Enable ADC
  enable_ADC();

  sei();
}

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

  // Program params (starts on white-red twinkle)
  command = IR_7;
  lightMode.currMode = TWINKLE;
  lightMode.prevMode = TWINKLE;
  twinkleParams.twinkleLEDOffset = 0;
  twinkleParams.twinkleChange = false;
  lightMode.modeChange = true; // Force set flag to execute default command
  brightnessChanged = false;
}

void setup() {
  // Initial setups
  setup_receiver_and_interrupts();
  setup_timer1();
  setup_ADC();
  set_initial_values();

  // Neopixels startup
  pixelsWide.begin();
  pixelsNarrow.begin();
}

// Selects 1 out of 10 random colors
uint16_t get_random_color() {
  return (rand() % 10) * HUE_STEP;
}

// Only affects the current selection of LEDs
void change_brightness(direction dir) {
  if (wideStripParams.selected) {
    // Adjust step dynamically (brightness changes below 50 are perceived better)
    uint8_t step = (wideStripParams.brightness > 50) ? LARGE_BRIGHTNESS_STEP :
                    (wideStripParams.brightness == 50 && dir == DECREASE) ? SMALL_BRIGHTNESS_STEP :
                      (wideStripParams.brightness < 50) ? SMALL_BRIGHTNESS_STEP : LARGE_BRIGHTNESS_STEP;

    if (dir == INCREASE) {
      wideStripParams.brightness = min(wideStripParams.brightness + step, MAX_BRIGHTNESS);
    }

    if (dir == DECREASE) {
      wideStripParams.brightness = max(wideStripParams.brightness - step, MIN_BRIGHTNESS);
    }
  }

  if (narrowStripParams.selected) {
    // Adjust step dynamically (brightness changes below 50 are perceived better)
    uint8_t step = (narrowStripParams.brightness > 50) ? LARGE_BRIGHTNESS_STEP :
                    (narrowStripParams.brightness == 50 && dir == DECREASE) ? SMALL_BRIGHTNESS_STEP :
                      (narrowStripParams.brightness < 50) ? SMALL_BRIGHTNESS_STEP : LARGE_BRIGHTNESS_STEP;

    if (dir == INCREASE) {
      narrowStripParams.brightness = min(narrowStripParams.brightness + step, MAX_BRIGHTNESS);
    } 

    if (dir == DECREASE) {
      narrowStripParams.brightness = max(narrowStripParams.brightness - step, MIN_BRIGHTNESS);
    }
  }

  // Change mode to actually apply the changes
  if (lightMode.currMode == NOTHING) {
    lightMode.currMode = STATIC;
  }
}

// Only affects the current selection of LEDs
void change_color(direction dir) {
  if (wideStripParams.selected) {
    if (dir == INCREASE) {
      wideStripParams.hue = (wideStripParams.hue + HUE_STEP) % MAX_HUE;
    }

    if (dir == DECREASE) {
      wideStripParams.hue = (wideStripParams.hue - HUE_STEP) % MAX_HUE;
    }

    wideStripParams.saturation = SATURATION_COLOR;
  }

  if (narrowStripParams.selected) {
    if (dir == INCREASE) {
      narrowStripParams.hue = (narrowStripParams.hue + HUE_STEP) % MAX_HUE;
    } 

    if (dir == DECREASE) {
      narrowStripParams.hue = (narrowStripParams.hue - HUE_STEP) % MAX_HUE;
    }

    narrowStripParams.saturation = SATURATION_COLOR;
  }

  // Change mode to actually apply the changes
  if (lightMode.currMode == NOTHING) {
    lightMode.currMode = STATIC;
  }
}

// Both LEDs will be static colored
void static_mode() {
  // Clear previously selected values
  pixelsWide.clear();
  pixelsNarrow.clear();

  // Set brightness
  pixelsWide.setBrightness(wideStripParams.brightness);
  pixelsNarrow.setBrightness(narrowStripParams.brightness);

  // Set color (transform HSV spectrum to RGB)
  for (int i = 0; i < NUM_PIXELS; i++) {
    pixelsWide.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(wideStripParams.hue, wideStripParams.saturation));
    pixelsNarrow.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(narrowStripParams.hue, narrowStripParams.saturation));
  }

  // Apply changes
  pixelsWide.show();
  pixelsNarrow.show();
}

void _execute_twinkle() {
  // Update LEDs values
  pixelsWide.setBrightness(wideStripParams.brightness);
  pixelsNarrow.setBrightness(narrowStripParams.brightness);

  // Update each individual pixel values
  for (uint8_t i = 0; i < NUM_PIXELS; i++) {
    // Narrow strip is always cycling
    pixelsNarrow.setPixelColor((i + twinkleParams.twinkleLEDOffset) % NUM_PIXELS, Adafruit_NeoPixel::ColorHSV(narrowStripParams.hue, narrowStripParams.saturation, pixelsNarrow.gamma8(i * (255 / NUM_PIXELS))));
    // Wide strip might be static
    if (wideStripParams.twinkle) {
      pixelsWide.setPixelColor((i + twinkleParams.twinkleLEDOffset) % NUM_PIXELS, Adafruit_NeoPixel::ColorHSV(wideStripParams.hue, wideStripParams.saturation, pixelsWide.gamma8(i * (255 / NUM_PIXELS))));
    } else {
      pixelsWide.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(wideStripParams.hue, wideStripParams.saturation));
    }
  }

  // Apply changes
  pixelsWide.show();
  pixelsNarrow.show();

  // Increase hue values for rainbow effect
  if (wideStripParams.rainbow) {
    wideStripParams.hue += HUE_TWINKLE_STEP;
    wideStripParams.hue %= MAX_HUE;
  }

  if (narrowStripParams.rainbow) {
    narrowStripParams.hue += HUE_TWINKLE_STEP;
    narrowStripParams.hue %= MAX_HUE;
  }
}

void twinkle_mode() {
  // Interrupt signaled it's time to update the twinkle effect
  if (twinkleParams.twinkleChange) {
    twinkleParams.twinkleChange = false;
    // Increase blacked out LED position on ring
    twinkleParams.twinkleLEDOffset = (twinkleParams.twinkleLEDOffset + 1) % NUM_PIXELS;
    _execute_twinkle();
  }
}

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

// Enables or disables the timer1 depending on the lighting mode
void update_timer_status() {
  // Enable timer when twinkle mode is selected
  if (lightMode.currMode == TWINKLE && lightMode.prevMode != TWINKLE) {
    start_twinkle_timer();
  }

  // Disable timer when twinkle mode is changed
  if (lightMode.prevMode == TWINKLE && lightMode.currMode != TWINKLE) {
    stop_twinkle_timer();
  }
}

// Enables or disables the ADC depending on the lighting mode
void update_ADC_status() {
  // Enable ADC when music mode is selected
  if (lightMode.currMode == MUSIC && lightMode.prevMode != MUSIC) {
    // Save previous brightness values
    wideStripParams.brightness_save = wideStripParams.brightness;
    narrowStripParams.brightness_save = narrowStripParams.brightness;

    // Start timer for ADC conversions
    start_music_timer();
  }

  // Disable ADC when music mode is changed
  if (lightMode.currMode != MUSIC && lightMode.prevMode == MUSIC) {
    // Restore previous brightness values
    wideStripParams.brightness = wideStripParams.brightness_save;
    narrowStripParams.brightness = narrowStripParams.brightness_save;

    // Disable timer for ADC conversions
    stop_music_timer();
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

void loop() {
  // Flag set from interrupt when new command is received
  if (lightMode.modeChange) {
    // Clear flag
    lightMode.modeChange = false;
    // Handle new command from interrupt
    decode_command();
  }

  // Update LEDs based on selected light mode
  execute_mode();
}

// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
// https://github.com/Arduino-IRremote/Arduino-IRremote#timer-and-pin-usage
