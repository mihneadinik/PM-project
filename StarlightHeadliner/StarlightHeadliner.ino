#include "adaptedTinyIRReceiver.hpp"
#include <Adafruit_NeoPixel.h>

// Arduino pins
#define IR_PIN 2
#define WIDE_PIN 6
#define NARROW_PIN 7

// Constants
const uint8_t NUM_PIXELS = 7;
const uint8_t MAX_BRIGHTNESS = 250;
const uint8_t MIN_BRIGHTNESS = 0;
const uint8_t SMALL_BRIGHTNESS_STEP = 10;
const uint8_t LARGE_BRIGHTNESS_STEP = 25;
const uint8_t SATURATION_WHITE = 0;
const uint8_t SATURATION_COLOR = 255;
const uint8_t VALUE_COLOR = 255;
const uint8_t VALUE_BLACK = 0;
const uint16_t MAX_HUE = 65535; // 16 bits max
const uint16_t TWINKLE_DELAY = 250; // can go up to 1048ms for 1/256 prescaler
const uint16_t MUSIC_CAPTURE_DELAY = 100; // can go up to 1048ms for 1/256 prescaler
const uint16_t TIMER_TWINKLE_COMPARE = (F_CPU / 256 / 1000 * TWINKLE_DELAY);
const uint16_t TIMER_MUSIC_COMPARE = (F_CPU / 256 / 1000 * MUSIC_CAPTURE_DELAY);
const uint8_t CYCLE_FADE_VALUE = (255 / NUM_PIXELS);

// Color constants
const uint16_t HUE_RED = 0;
const uint16_t HUE_YELLOW = 1 * (MAX_HUE / 6);
const uint16_t HUE_GREEN = 2 * (MAX_HUE / 6);
const uint16_t HUE_CIAN = 3 * (MAX_HUE / 6);
const uint16_t HUE_BLUE = 4 * (MAX_HUE / 6);
const uint16_t HUE_MAGENTA = 5 * (MAX_HUE / 6);
const uint16_t HUE_STEP = MAX_HUE / 10;
const uint16_t HUE_TWINKLE_STEP = MAX_HUE / 500;

// Button decoded values
#define IR_1 69
#define IR_2 70
#define IR_3 71
#define IR_4 68
#define IR_5 64
#define IR_6 67
#define IR_7 7
#define IR_8 21
#define IR_9 9
#define IR_STAR 22
#define IR_0 25
#define IR_HASHTAG 13
#define IR_UP 24
#define IR_LEFT 8
#define IR_OK 28
#define IR_RIGHT 90
#define IR_DOWN 82

// LED states
enum state {STATIC, TWINKLE, MUSIC, NOTHING};

// used for brightnes and color changes
enum direction {INCREASE, DECREASE};

// Neopixels
Adafruit_NeoPixel pixelsWide(NUM_PIXELS, WIDE_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixelsNarrow(NUM_PIXELS, NARROW_PIN, NEO_GRB + NEO_KHZ800);

// Neopixels values
uint8_t saturationWide, brightnessWide, brightnessWide_copy;
uint16_t hueWide;
uint8_t saturationNarrow, brightnessNarrow, brightnessNarrow_copy;
uint16_t hueNarrow;

// Program values
uint8_t command;
state prevMode, currMode;
uint8_t twinkleOffset;
bool modeChange;
bool twinkleChange;
bool selectWide, selectNarrow;
bool rainbowWide, rainbowNarrow;
bool twinkleWide, twinkleNarrow;

// Music reactive values
bool musicEnabled, brightnessChanged;

// Interrupt routine for twinkle effect
ISR(TIMER1_COMPA_vect) {
  twinkleChange = true;
}

// Interrupt routine for music input capture
ISR(TIMER1_COMPB_vect) {
  // Start ADC conversion
  ADCSRA |= (1 << ADSC);
}

// Interrupt routine ADC
ISR(ADC_vect) {
  // Read conversion and set brightness accordingly
  uint8_t externNoise = (ADCH > 10) ? ADCH : 10;
  Serial.println(externNoise);

  uint8_t brightnessNarrow_new = externNoise;
  // Add a random brightness increase
  uint8_t brightnessWide_new = (externNoise + ((externNoise > 10) ? 10 + rand() % 30 : 0)) % MAX_BRIGHTNESS;
  
  // Only change with 3 quarters of the difference for smoothness
  if (brightnessNarrow_new > brightnessNarrow) {
    brightnessNarrow = (uint8_t)(brightnessNarrow + (brightnessNarrow_new - brightnessNarrow) * 0.75) % MAX_BRIGHTNESS;
  } else {
    brightnessNarrow = (uint8_t)(brightnessNarrow - (brightnessNarrow - brightnessNarrow_new) * 0.75) % MAX_BRIGHTNESS;
  }

  if (brightnessWide_new > brightnessWide) {
    brightnessWide = (uint8_t)(brightnessWide + (brightnessWide_new - brightnessWide) * 0.75) % MAX_BRIGHTNESS;
  } else {
    brightnessWide = (uint8_t)(brightnessWide - (brightnessWide - brightnessWide_new) * 0.75) % MAX_BRIGHTNESS;
  }

  // Set flag to update brightness
  brightnessChanged = true;
}

// Callback after ISR routine on IR_PIN is over
void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags) {
  // Save received command
  command = aCommand;
  // Set flag to decode it
  modeChange = true;
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
  hueWide = HUE_RED;
  saturationWide = SATURATION_COLOR;
  brightnessWide = MAX_BRIGHTNESS;
  hueNarrow = HUE_RED;
  saturationNarrow = SATURATION_COLOR;
  brightnessNarrow = MAX_BRIGHTNESS;

  // Program params (starts on white-red twinkle)
  command = IR_7;
  currMode = TWINKLE;
  prevMode = TWINKLE;
  twinkleOffset = 0;
  modeChange = true;
  twinkleChange = false;
  selectWide = true;
  selectNarrow = true;
  rainbowWide = false;
  rainbowNarrow = false;
  twinkleWide = false;
  twinkleNarrow = false;
  musicEnabled = false;
  brightnessChanged = false;
}

void setup() {
  Serial.begin(9600);
  // Initial setups
  setup_receiver_and_interrupts();
  setup_timer1();
  setup_ADC();
  set_initial_values();

  Serial.println("Starting");

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
  if (selectWide) {
    // Adjust step dynamically (brightness changes below 50 are perceived better)
    uint8_t step = (brightnessWide > 50) ? LARGE_BRIGHTNESS_STEP :
                    (brightnessWide == 50 && dir == DECREASE) ? SMALL_BRIGHTNESS_STEP :
                      (brightnessWide < 50) ? SMALL_BRIGHTNESS_STEP : LARGE_BRIGHTNESS_STEP;

    if (dir == INCREASE) {
      brightnessWide = min(brightnessWide + step, MAX_BRIGHTNESS);
    }

    if (dir == DECREASE) {
      brightnessWide = max(brightnessWide - step, MIN_BRIGHTNESS);
    }
  }

  if (selectNarrow) {
    // Adjust step dynamically (brightness changes below 50 are perceived better)
    uint8_t step = (brightnessNarrow > 50) ? LARGE_BRIGHTNESS_STEP :
                    (brightnessNarrow == 50 && dir == DECREASE) ? SMALL_BRIGHTNESS_STEP :
                      (brightnessNarrow < 50) ? SMALL_BRIGHTNESS_STEP : LARGE_BRIGHTNESS_STEP;

    if (dir == INCREASE) {
      brightnessNarrow = min(brightnessNarrow + step, MAX_BRIGHTNESS);
    } 

    if (dir == DECREASE) {
      brightnessNarrow = max(brightnessNarrow - step, MIN_BRIGHTNESS);
    }
  }

  // Change mode to actually apply the changes
  if (currMode == NOTHING) {
    currMode = STATIC;
  }
}

// Only affects the current selection of LEDs
void change_color(direction dir) {
  if (selectWide) {
    if (dir == INCREASE) {
      hueWide = (hueWide + HUE_STEP) % MAX_HUE;
    }

    if (dir == DECREASE) {
      hueWide = (hueWide - HUE_STEP) % MAX_HUE;
    }

    saturationWide = SATURATION_COLOR;
  }

  if (selectNarrow) {
    if (dir == INCREASE) {
      hueNarrow = (hueNarrow + HUE_STEP) % MAX_HUE;
    } 

    if (dir == DECREASE) {
      hueNarrow = (hueNarrow - HUE_STEP) % MAX_HUE;
    }

    saturationNarrow = SATURATION_COLOR;
  }

  // Change mode to actually apply the changes
  if (currMode == NOTHING) {
    currMode = STATIC;
  }
}

// Both LEDs will be static colored
void static_mode() {
  // Clear previously selected values
  pixelsWide.clear();
  pixelsNarrow.clear();

  // Set brightness
  pixelsWide.setBrightness(brightnessWide);
  pixelsNarrow.setBrightness(brightnessNarrow);

  // Set color (transform HSV spectrum to RGB)
  for (int i = 0; i < NUM_PIXELS; i++) {
    pixelsWide.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(hueWide, saturationWide));
    pixelsNarrow.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(hueNarrow, saturationNarrow));
  }

  // Apply changes
  pixelsWide.show();
  pixelsNarrow.show();
}

void _execute_twinkle() {
  // Update LEDs values
  pixelsWide.setBrightness(brightnessWide);
  pixelsNarrow.setBrightness(brightnessNarrow);

  // Update each individual pixel values
  for (uint8_t i = 0; i < NUM_PIXELS; i++) {
    // Narrow strip is always cycling
    pixelsNarrow.setPixelColor((i + twinkleOffset) % NUM_PIXELS, Adafruit_NeoPixel::ColorHSV(hueNarrow, saturationNarrow, pixelsNarrow.gamma8(i * (255 / NUM_PIXELS))));
    // Wide strip might be static
    if (twinkleWide) {
      pixelsWide.setPixelColor((i + twinkleOffset) % NUM_PIXELS, Adafruit_NeoPixel::ColorHSV(hueWide, saturationWide, pixelsWide.gamma8(i * (255 / NUM_PIXELS))));
    } else {
      pixelsWide.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(hueWide, saturationWide));
    }
  }

  // Apply changes
  pixelsWide.show();
  pixelsNarrow.show();

  // Increase hue values for rainbow effect
  if (rainbowWide) {
    hueWide += HUE_TWINKLE_STEP;
    hueWide %= MAX_HUE;
  }

  if (rainbowNarrow) {
    hueNarrow += HUE_TWINKLE_STEP;
    hueNarrow %= MAX_HUE;
  }
}

void twinkle_mode() {
  // Interrupt signaled it's time to update the twinkle effect
  if (twinkleChange) {
    twinkleChange = false;
    // Increase blacked out LED position on ring
    twinkleOffset = (twinkleOffset + 1) % NUM_PIXELS;
    _execute_twinkle();
  }
}

// Applies changes to the LEDs
void execute_mode() {
  switch (currMode) {
    case STATIC:
      static_mode();
      // Change mode to blank state after applying changes
      currMode = NOTHING;
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
  if (currMode == TWINKLE && prevMode != TWINKLE) {
    start_twinkle_timer();
  }

  // Disable timer when twinkle mode is changed
  if (prevMode == TWINKLE && currMode != TWINKLE) {
    stop_twinkle_timer();
  }
}

// Enables or disables the ADC depending on the lighting mode
void update_ADC_status() {
  // Enable ADC when music mode is selected
  if (currMode == MUSIC && prevMode != MUSIC) {
    // Save previous brightness values
    brightnessWide_copy = brightnessWide;
    brightnessNarrow_copy = brightnessNarrow;

    // Set music mode flag and start its timer
    musicEnabled = true;
    start_music_timer();
  }

  // Disable ADC when music mode is changed
  if (musicEnabled && currMode != MUSIC) {
    // Restore previous brightness values
    brightnessWide = brightnessWide_copy;
    brightnessNarrow = brightnessNarrow_copy;

    // Clear music mode flag and stop its timer
    musicEnabled = false;
    stop_music_timer();
  }
}

// Decodes the code received from remote
void decode_command() {
  // Save last state
  prevMode = currMode;

  switch (command) {
    // Both static white color mode
    case IR_1:
      // Set white color (only need max saturation)
      saturationWide = SATURATION_WHITE;
      saturationNarrow = SATURATION_WHITE;

      // Clear rainbow and twinkle flags
      twinkleWide = false;
      twinkleNarrow = false;
      rainbowWide = false;
      rainbowNarrow = false;

      // Update current light mode
      currMode = STATIC;
      break;
    
    // Both static red color mode
    case IR_2:
      // Set color specific saturation value
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;
      // Set red color 
      hueWide = HUE_RED;
      hueNarrow = HUE_RED;

      // Clear rainbow and twinkle flags
      twinkleWide = false;
      twinkleNarrow = false;
      rainbowWide = false;
      rainbowNarrow = false;

      // Update current light mode
      currMode = STATIC;
      break;

    // Both static random color mode
    case IR_3:
      // Set color specific saturation value
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;
      // Choose a random color
      hueWide = hueNarrow = get_random_color();

      // Clear rainbow and twinkle flags
      twinkleWide = false;
      twinkleNarrow = false;
      rainbowWide = false;
      rainbowNarrow = false;

      // Update current light mode
      currMode = STATIC;
      break;
    
    // Both twinkle white color mode
    case IR_4:
      // Set white color (only need max saturation)
      saturationWide = SATURATION_WHITE;
      saturationNarrow = SATURATION_WHITE;

      // Set twinkle flag (both strips) and clear rainbow flag
      twinkleWide = true;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      // update current light mode
      currMode = TWINKLE;
      break;
    
    // Both twinkle red color mode
    case IR_5:
      // Set color specific saturation value
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;
      // Set red color
      hueWide = HUE_RED;
      hueNarrow = HUE_RED;
      
      // Set twinkle flag (both strips) and clear rainbow flag
      twinkleWide = true;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      // update current light mode
      currMode = TWINKLE;
      break;

    // Both twinkle rainbow mode
    case IR_6:
      // Set color specific saturation value
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;

      // Set twinkle (both strips) and rainbow flags
      twinkleWide = true;
      twinkleNarrow = true;
      rainbowWide = true;
      rainbowNarrow = true;

      // Update current light mode
      currMode = TWINKLE;
      break;

    // Single (narrow) white twinkle with static red color mode
    case IR_7:
      // Set colors
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_WHITE;
      hueWide = HUE_RED;

      // Set twinkle (one strip) flag and clear rainbow flag
      twinkleWide = false;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      // Update current light mode
      currMode = TWINKLE;
      break;
    
    // Single (narrow) white twinkle with static random color mode
    case IR_8:
      // Set colors
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_WHITE;
      hueWide = get_random_color();

      // Set twinkle (one strip) flag and clear rainbow flag
      twinkleWide = false;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      // Update current light mode
      currMode = TWINKLE;
      break;

    // Single (narrow) white twinkle with rainbow color mode
    case IR_9:
      // Set colors
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_WHITE;

      // Set twinkle (one strip) flag and set rainbow flag
      twinkleWide = false;
      twinkleNarrow = true;
      rainbowWide = true;
      rainbowNarrow = false;

      // update current light mode
      currMode = TWINKLE;
      break;

    // Select wide only (for brightness or colour change)
    case IR_STAR:
      selectWide = true;
      selectNarrow = false;
      break;
    
    // Select both (for brightness or colour change)
    case IR_0:
      selectWide = true;
      selectNarrow = true;
      break;

    // Select narrow only (for brightness or colour change)
    case IR_HASHTAG:
      selectNarrow = true;
      selectWide = false;
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
      currMode = MUSIC;
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
  if (modeChange) {
    // Clear flag
    modeChange = false;
    // Handle new command from interrupt
    decode_command();
  }

  // Update LEDs based on selected light mode
  execute_mode();
}

// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
// https://github.com/Arduino-IRremote/Arduino-IRremote#timer-and-pin-usage
