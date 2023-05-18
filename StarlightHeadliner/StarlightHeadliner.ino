#include "adaptedTinyIRReceiver.hpp"
#include <Adafruit_NeoPixel.h>

// Arduino pins
#define IR_PIN 2
#define WIDE_PIN 6
#define NARROW_PIN 7

// Constants
#define NUM_PIXELS 7
#define MAX_BRIGHTNESS 250
#define MIN_BRIGHTNESS 0
#define SMALL_BRIGHTNESS_STEP 10
#define LARGE_BRIGHTNESS_STEP 25
#define SATURATION_WHITE 0
#define SATURATION_COLOR 255
#define VALUE_COLOR 255
#define VALUE_BLACK 0
#define MAX_HUE 65535 // 16 bits max
#define TWINKLE_DELAY 250
#define MUSIC_CAPTURE_DELAY 100
#define TIMER_TWINKLE_COMPARE (F_CPU / 256 / 1000 * TWINKLE_DELAY)
#define TIMER_MUSIC_COMPARE (F_CPU / 256 / 1000 * MUSIC_CAPTURE_DELAY)
#define CYCLE_FADE_VALUE (255 / NUM_PIXELS)

// Color constants
#define HUE_RED 0
#define HUE_YELLOW 1 * (MAX_HUE / 6)
#define HUE_GREEN 2 * (MAX_HUE / 6)
#define HUE_CIAN 3 * (MAX_HUE / 6)
#define HUE_BLUE 4 * (MAX_HUE / 6)
#define HUE_MAGENTA 5 * (MAX_HUE / 6)
#define HUE_STEP MAX_HUE / 10
#define HUE_TWINKLE_STEP MAX_HUE / 500

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

// Music reactive
uint8_t externNoise;
bool musicEnabled, brightnessChanged;

// Interrupt routine for twinkle effect
ISR(TIMER1_COMPA_vect) {
  twinkleChange = true;
}

// Interrupt routine for music input capture
ISR(TIMER1_COMPB_vect) {
  // start ADC conversion
  ADCSRA |= (1 << ADSC);
}

// Interrupt routine ADC
ISR(ADC_vect) {
  // read conversion and set brightness accordingly
  externNoise = (ADCH > 10) ? ADCH : 10;

  uint8_t brightnessNarrow_new = externNoise;
  // add a random brightness increase
  uint8_t brightnessWide_new = (externNoise + ((externNoise > 10) ? 10 + rand() % 30 : 0)) % MAX_BRIGHTNESS;
  
  // only change with 3 quarters of the difference for smoothness
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

  // set flag to update brightness
  brightnessChanged = true;
}

// callback after ISR routine on IR_PIN is over
void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags) {
  command = aCommand;
  modeChange = true;
}

// sets the ADC to Free Running mode
void setup_ADC() {
  cli();

  ADCSRB = 0;
  ADCSRA = 0;
  ADCSRA |= (1 << ADIE);  // enable ADC interrupts

  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // Setează prescaler la 128 pentru a obține o frecvență de eșantionare corespunzătoare

  ADMUX |= (1 << REFS0) | (1 << REFS1);  // Setează referința de tensiune la 1.1V
  ADMUX |= (1 << ADLAR);  // Setează rezultatul ADC să fie aliniat la stânga (8 biți)

  sei();
}

void enable_ADC() {
  // enable ADC
  ADCSRA |= (1 << ADEN);
}

void disable_ADC() {
  // disable ADC
  ADCSRA &= ~(1 << ADEN);
}

// sets timer1 to compare match on both channels
void setup_timer1() {
  cli();

  // clear the registers
  TCNT1 = 0;
  TCCR1A = 0;
  TCCR1B = 0;
  // set the timer to stop on compare match
  TCCR1A = 0;
  TCCR1B |= (1 << WGM12);
  // set prescaler to 256
  TCCR1B |= (1 << CS12);
  // set compare value
  OCR1A = TIMER_TWINKLE_COMPARE;
  OCR1B = TIMER_MUSIC_COMPARE;
  // activate interrupt on compare match
  TIMSK1 |= (1 << OCIE1A);

  sei();
}

// stops counting for twkinkle changes
void stop_twinkle_timer() {
  cli();

  TIMSK1 &= ~(1 << OCIE1A);
  Serial.println("Twinkle end");
  
  sei();
}

// stops counting for music changes
void stop_music_timer() {
  cli();

  TIMSK1 &= ~(1 << OCIE1B);
  disable_ADC();
  Serial.println("Music end");
  
  sei();
}

// restarts the twinkle counting
void start_twinkle_timer() {
  cli();

  TIMSK1 |= (1 << OCIE1A);
  Serial.println("Twinkle start");

  sei();
}

// restarts the music capture counting
void start_music_timer() {
  cli();

  TIMSK1 |= (1 << OCIE1B);
  enable_ADC();
  Serial.println("Music start");

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
  // initial setups
  setup_receiver_and_interrupts();
  setup_timer1();
  setup_ADC();
  set_initial_values();

  Serial.println("Starting");

  // Neopixels startup
  pixelsWide.begin();
  pixelsNarrow.begin();
}

uint16_t get_random_color() {
  return (rand() % 10) * HUE_STEP;
}

// only affects the current selection of LEDs
void change_brightness(direction dir) {
  if (selectWide) {
    // adjust step dynamically
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
    // adjust step dynamically
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

  // change mode to actually apply the changes
  if (currMode == NOTHING) {
    currMode = STATIC;
  }
}

// only affects the current selection of LEDs
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

  // change mode to actually apply the changes
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

  for (int i = 0; i < NUM_PIXELS; i++) {
    // Narrow strip is always cycling
    pixelsNarrow.setPixelColor((i + twinkleOffset) % NUM_PIXELS, Adafruit_NeoPixel::ColorHSV(hueNarrow, saturationNarrow, pixelsNarrow.gamma8(i * (255 / NUM_PIXELS))));
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
    // increase blacked out LED position on ring
    twinkleOffset = (twinkleOffset + 1) % NUM_PIXELS;
    _execute_twinkle();
  }
}

// Applies changes to the LEDs
void execute_mode() {
  switch (currMode) {
    case STATIC:
      // Serial.println("STATIC");
      static_mode();
      // change mode to blank state after applying changes
      currMode = NOTHING;
      break;
    
    case TWINKLE:
      // Serial.println("TWINKLE");
      twinkle_mode();
      break;

    case MUSIC:
      // Serial.println("MUSIC");
      if (brightnessChanged) {
        // update brightness
        brightnessChanged = false;
        static_mode();
      }
      break;
    
    case NOTHING:
      break;
  }
}

// Enables or disables the timer1 depending on the lighting mode
void update_timer_status() {
  // Enable timer
  if (currMode == TWINKLE && prevMode != TWINKLE) {
    start_twinkle_timer();
  }

  // Disable timer
  if (prevMode == TWINKLE && currMode != TWINKLE) {
    stop_twinkle_timer();
  }
}

// Enables or disables the ADC depending on the lighting mode
void update_ADC_status() {
  // Enable ADC
  if (currMode == MUSIC && prevMode != MUSIC) {
    Serial.println("Enabled ADC");
    brightnessWide_copy = brightnessWide;
    brightnessNarrow_copy = brightnessNarrow;
    musicEnabled = true;
    start_music_timer();
  }

  // Disable ADC
  if (musicEnabled && currMode != MUSIC) {
    Serial.println("Disabled ADC");
    brightnessWide = brightnessWide_copy;
    brightnessNarrow = brightnessNarrow_copy;
    musicEnabled = false;
    stop_music_timer();
  }
}

// Decodes the code received from remote
void decode_command() {
  // save last state
  prevMode = currMode;

  switch (command) {
    // Both static white color mode
    case IR_1:
      Serial.println("IR_1");
      saturationWide = SATURATION_WHITE;
      saturationNarrow = SATURATION_WHITE;

      twinkleWide = false;
      twinkleNarrow = false;
      rainbowWide = false;
      rainbowNarrow = false;

      currMode = STATIC;
      break;
    
    // Both static red color mode
    case IR_2:
      Serial.println("IR_2");
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;
      hueWide = HUE_RED;
      hueNarrow = HUE_RED;

      twinkleWide = false;
      twinkleNarrow = false;
      rainbowWide = false;
      rainbowNarrow = false;

      currMode = STATIC;
      break;

    // Both static random color mode
    case IR_3:
      Serial.println("IR_3");
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;
      hueWide = hueNarrow = get_random_color();

      twinkleWide = false;
      twinkleNarrow = false;
      rainbowWide = false;
      rainbowNarrow = false;

      currMode = STATIC;
      break;
    
    // Both twinkle white color mode
    case IR_4:
      Serial.println("IR_4");
      saturationWide = SATURATION_WHITE;
      saturationNarrow = SATURATION_WHITE;

      twinkleWide = true;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      currMode = TWINKLE;
      break;
    
    // Both twinkle red color mode
    case IR_5:
      Serial.println("IR_5");
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;
      hueWide = HUE_RED;
      hueNarrow = HUE_RED;
      
      twinkleWide = true;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      currMode = TWINKLE;
      break;

    // Both twinkle rainbow mode
    case IR_6:
      Serial.println("IR_6");
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_COLOR;

      twinkleWide = true;
      twinkleNarrow = true;
      rainbowWide = true;
      rainbowNarrow = true;

      currMode = TWINKLE;
      break;

    // Single (narrow) white twinkle with static red color mode
    case IR_7:
      Serial.println("IR_7");
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_WHITE;
      hueWide = HUE_RED;

      twinkleWide = false;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      currMode = TWINKLE;
      break;
    
    // Single (narrow) white twinkle with static random color mode
    case IR_8:
      Serial.println("IR_8");
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_WHITE;
      hueWide = get_random_color();

      twinkleWide = false;
      twinkleNarrow = true;
      rainbowWide = false;
      rainbowNarrow = false;

      currMode = TWINKLE;
      break;

    // Single (narrow) white twinkle with rainbow color mode
    case IR_9:
      Serial.println("IR_9");
      saturationWide = SATURATION_COLOR;
      saturationNarrow = SATURATION_WHITE;

      twinkleWide = false;
      twinkleNarrow = true;
      rainbowWide = true;
      rainbowNarrow = false;

      currMode = TWINKLE;
      break;

    // Select wide only
    case IR_STAR:
      Serial.println("IR_STAR");
      selectWide = true;
      selectNarrow = false;
      break;
    
    // Select both
    case IR_0:
      Serial.println("IR_0");
      selectWide = true;
      selectNarrow = true;
      break;

    // Select narrow only
    case IR_HASHTAG:
      Serial.println("IR_HASHTAG");
      selectNarrow = true;
      selectWide = false;
      break;

    // Increase brightness on selection
    case IR_UP:
      Serial.println("IR_UP");
      change_brightness(INCREASE);
      break;
    
    // Change color counter-clockwise on selection
    case IR_LEFT:
      Serial.println("IR_LEFT");
      change_color(DECREASE);
      break;

    // Music reactive mode
    case IR_OK:
      Serial.println("IR_OK");
      currMode = MUSIC;
      break;

    // Change color clockwise on selection
    case IR_RIGHT:
      Serial.println("IR_RIGHT");
      change_color(INCREASE);
      break;
    
    // Decrease brightness on selection
    case IR_DOWN:
      Serial.println("IR_DOWN");
      change_brightness(DECREASE);
      break;
  }

  update_timer_status();
  update_ADC_status();
}

void loop() {
  if (modeChange) {
    // handle new command from interrupt
    modeChange = false;
    decode_command();
  }

  execute_mode();
}

// https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
// https://github.com/Arduino-IRremote/Arduino-IRremote#timer-and-pin-usage
