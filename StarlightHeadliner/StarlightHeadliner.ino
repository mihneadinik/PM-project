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
#define MAX_HUE 360
#define TWINKLE_DELAY 250
#define CYCLE_FADE_VALUE (255 / NUM_PIXELS)

// Color constants
#define HUE_RED 0
#define HUE_YELLOW 60
#define HUE_GREEN 120
#define HUE_CIAN 180
#define HUE_BLUE 240
#define HUE_MAGENTA 300

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
    IrReceiver.resume();
    modeChange = true;
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
}

uint32_t getPixelColorHsv(uint16_t h, uint8_t s, uint8_t v=255) {

  uint8_t r, g, b;

  if (!s) {
    // Monochromatic, all components are V
    r = g = b = v;
  } else {
    uint8_t sextant = h >> 8;
    if (sextant > 5)
      sextant = 5;  // Limit hue sextants to defined space

    g = v;    // Top level

    // Perform actual calculations

    /*
       Bottom level:
       --> (v * (255 - s) + error_corr + 1) / 256
    */
    uint16_t ww;        // Intermediate result
    ww = v * (uint8_t)(~s);
    ww += 1;            // Error correction
    ww += ww >> 8;      // Error correction
    b = ww >> 8;

    uint8_t h_fraction = h & 0xff;  // Position within sextant
    uint32_t d;      // Intermediate result

    if (!(sextant & 1)) {
      // r = ...slope_up...
      // --> r = (v * ((255 << 8) - s * (256 - h)) + error_corr1 + error_corr2) / 65536
      d = v * (uint32_t)(0xff00 - (uint16_t)(s * (256 - h_fraction)));
      d += d >> 8;  // Error correction
      d += v;       // Error correction
      r = d >> 16;
    } else {
      // r = ...slope_down...
      // --> r = (v * ((255 << 8) - s * h) + error_corr1 + error_corr2) / 65536
      d = v * (uint32_t)(0xff00 - (uint16_t)(s * h_fraction));
      d += d >> 8;  // Error correction
      d += v;       // Error correction
      r = d >> 16;
    }

    // Swap RGB values according to sextant. This is done in reverse order with
    // respect to the original because the swaps are done after the
    // assignments.
    if (!(sextant & 6)) {
      if (!(sextant & 1)) {
        uint8_t tmp = r;
        r = g;
        g = tmp;
      }
    } else {
      if (sextant & 1) {
        uint8_t tmp = r;
        r = g;
        g = tmp;
      }
    }
    if (sextant & 4) {
      uint8_t tmp = g;
      g = b;
      b = tmp;
    }
    if (sextant & 2) {
      uint8_t tmp = r;
      r = b;
      b = tmp;
    }
  }
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t HSV_to_RGB_convertor(uint16_t H, uint8_t S = 255, uint8_t V = 255) {
  uint8_t r = 0, g = 0, b = 0;

  uint16_t max = V;
  uint16_t C = S * V; // chroma value
  uint16_t min = max - C;
  uint16_t H_prime;

  if (H >= 300) {
    H_prime = (H - 360) / 60;
  } else {
    H_prime = H / 60;
  }

  Serial.println(H);
  Serial.println(H_prime);

  if (H_prime >= -1 && H_prime < 1) {
    r = max;
    if (H_prime < 0) {
      g = min;
      b = g - H_prime * C;
    } else {
      b = min;
      g = b + H_prime * C;
    }
  }

  if (H_prime >= 1 && H_prime < 3) {
    g = max;
    if (H_prime - 2 < 0) {
      b = min;
      r = b - (H_prime - 2) * C;
    } else {
      r = min;
      b = r + (H_prime - 2) * C;
    }
  }

  if (H_prime >= 3 && H_prime < 5) {
    b = max;
    if (H_prime - 4 < 0) {
      r = min;
      g = r - (H_prime - 4) * C;
    } else {
      g = min;
      r = g + (H_prime - 4) * C;
    }
  }

  // Serial.print(r);
  // Serial.print(" ");
  // Serial.print(g);
  // Serial.print(" ");
  // Serial.println(b);

  return Adafruit_NeoPixel::Color(r, g, b);
}

uint16_t get_random_color() {
  return rand() % MAX_HUE;
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
    pixelsWide.setPixelColor(i, HSV_to_RGB_convertor(hueWide, saturationWide));
    pixelsNarrow.setPixelColor(i, HSV_to_RGB_convertor(hueNarrow, saturationNarrow));
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
      hueWide = (hueWide + 20) % MAX_HUE;
    }
    if (dir == DECREASE) {
      hueWide = (hueWide - 20) % MAX_HUE;
    }
  }

  if (selectNarrow) {
    if (dir == INCREASE) {
      hueNarrow = (hueNarrow + 20) % MAX_HUE;
    } 
    if (dir == DECREASE) {
      hueNarrow = (hueNarrow - 20) % MAX_HUE;
    }
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
      Serial.println("STATIC");
      static_mode();
      // change mode to blank state after applying changes
      prevMode = NOTHING;
      break;
    
    case TWINKLE:
      Serial.println("TWINKLE");
      break;

    case MUSIC:
      Serial.println("MUSIC");
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

// https://cs.stackexchange.com/questions/64549/convert-hsv-to-rgb-colors
