#include <IRremote.h>
#include <Adafruit_NeoPixel.h>

// Arduino pins
#define IR_PIN 2
#define WIDE_PIN 6
#define NARROW_PIN 7

// Constants
#define NUM_PIXELS 7
#define MAX_BRIGHTNESS 250
#define MIN_BRIGHTNESS 0
#define SATURATION_WHITE 0
#define SATURATION_COLOR 255
#define VALUE_COLOR 255
#define VALUE_BLACK 0
#define MAX_HUE 65536 // 16 bits max
#define TWINKLE_DELAY 250
#define CYCLE_FADE_VALUE (255 / NUM_PIXELS)

// Color constants
#define HUE_RED 0
#define HUE_YELLOW 1 * (MAX_HUE / 6)
#define HUE_GREEN 2 * (MAX_HUE / 6)
#define HUE_CIAN 3 * (MAX_HUE / 6)
#define HUE_BLUE 4 * (MAX_HUE / 6)
#define HUE_MAGENTA 5 * (MAX_HUE / 6)
#define HUE_STEP MAX_HUE / 10

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
uint8_t saturationWide, brightnessWide;
uint16_t hueWide;
uint8_t saturationNarrow, brightnessNarrow;
uint16_t hueNarrow;

// Program values
uint16_t command;
state prevMode;
bool modeChange;
bool selectWide, selectNarrow;
bool rainbowWide, rainbowNarrow;
bool twinkleWide, twinkleNarrow;

// Interrupt routine for PD2
ISR(INT0_vect) {
  if (IrReceiver.decode()) {
    command = IrReceiver.decodedIRData.command;
    modeChange = true;
    IrReceiver.resume();
  }
}

// activates external interrupts for PD2
void setup_interrupts() {
  cli();

  // rising edge on PD2
  EICRA |= (1 << ISC01);
  EICRA |= (1 << ISC00);

  // activate interrupts on PD2
  EIMSK |= (1 << INT0);
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
  prevMode = TWINKLE;
  modeChange = true;
  // restoreState = false;
  selectWide = true;
  selectNarrow = true;
  rainbowWide = false;
  rainbowNarrow = false;
  twinkleWide = false;
  twinkleNarrow = false;
}

void setup() {
  // TODO change, only for getting 5V
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  Serial.begin(9600);
  setup_interrupts();
  set_initial_values();

  IrReceiver.begin(IR_PIN);
  Serial.println("Starting");

  pixelsWide.begin();
  pixelsNarrow.begin();
}

uint16_t get_random_color() {
  return (rand() % 10) * HUE_STEP;
}

void static_mode() {
  // clear previously selected values
  pixelsWide.clear();
  pixelsNarrow.clear();

  // set brightness
  pixelsWide.setBrightness(brightnessWide);
  pixelsNarrow.setBrightness(brightnessNarrow);

  // set color (transform HSV spectrum to RGB)
  for (int i = 0; i < NUM_PIXELS; i++) {
    pixelsWide.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(hueWide, saturationWide));
    pixelsNarrow.setPixelColor(i, Adafruit_NeoPixel::ColorHSV(hueNarrow, saturationNarrow));
  }

  // apply changes
  pixelsWide.show();
  pixelsNarrow.show();
}

// only affects the current selection of LEDs
void change_brightness(direction dir) {
  if (selectWide) {
    if (dir == INCREASE) {
      brightnessWide = min(brightnessWide + 25, MAX_BRIGHTNESS);
    }
    if (dir == DECREASE) {
      brightnessWide = max(brightnessWide - 25, MIN_BRIGHTNESS);
    }
  }

  if (selectNarrow) {
    if (dir == INCREASE) {
      brightnessNarrow = min(brightnessNarrow + 25, MAX_BRIGHTNESS);
    } 
    if (dir == DECREASE) {
      brightnessNarrow = max(brightnessNarrow - 25, MIN_BRIGHTNESS);
    }
  }

  // change mode to actually apply the changes
  if (prevMode == NOTHING) {
    prevMode = STATIC;
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
  if (prevMode == NOTHING) {
    prevMode = STATIC;
  }
}

// Applies changes to the LEDs
void execute_mode() {
  switch (prevMode) {
    case STATIC:
      // Serial.println("STATIC");
      static_mode();
      // change mode to blank state after applying changes
      prevMode = NOTHING;
      break;
    
    case TWINKLE:
      // Serial.println("TWINKLE");
      break;

    case MUSIC:
      // Serial.println("MUSIC");
      break;
    
    case NOTHING:
      break;
  }
}

// Decodes the code received from remote
void decode_command() {
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

      prevMode = STATIC;
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

      prevMode = STATIC;
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

      prevMode = STATIC;
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

      prevMode = TWINKLE;
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

      prevMode = TWINKLE;
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

      prevMode = TWINKLE;
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

      prevMode = TWINKLE;
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

      prevMode = TWINKLE;
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

      prevMode = TWINKLE;
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
