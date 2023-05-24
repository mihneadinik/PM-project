#ifndef _CONSTANTS_AND_TYPES_H
#define _CONSTANTS_AND_TYPES_H

/*************************************************************************************************\
 *                                        Constant values                                        *
\*************************************************************************************************/

// Program constants
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
const uint8_t IR_1 = 69;
const uint8_t IR_2 = 70;
const uint8_t IR_3 = 71;
const uint8_t IR_4 = 68;
const uint8_t IR_5 = 64;
const uint8_t IR_6 = 67;
const uint8_t IR_7 = 7;
const uint8_t IR_8 = 21;
const uint8_t IR_9 = 9;
const uint8_t IR_STAR = 22;
const uint8_t IR_0 = 25;
const uint8_t IR_HASHTAG = 13;
const uint8_t IR_UP = 24;
const uint8_t IR_LEFT = 8;
const uint8_t IR_OK = 28;
const uint8_t IR_RIGHT = 90;
const uint8_t IR_DOWN = 82;

/*************************************************************************************************\
 *                                             Enums                                             *
\*************************************************************************************************/

// LED states
enum state {STATIC, TWINKLE, MUSIC, NOTHING};

// Used for brightness and color changes
enum direction {INCREASE, DECREASE};

/*************************************************************************************************\
 *                                        Data structures                                        *
\*************************************************************************************************/

// Structure used to keep runtime parameters of LEDs
typedef struct {
  uint8_t saturation;
  uint8_t brightness;
  uint8_t brightness_save; // Helps restore previous value after music mode
  uint16_t hue;
  bool selected; // Changes of color and brightness will only apply if true
  bool rainbow; // Color cycling will only apply if true
  bool twinkle; // Brightness cycling will only apply if true
} stripParams_t;

// Structure used to keep runtime parameters of twinkle mode
typedef struct {
  uint8_t twinkleLEDOffset; // Blacked out LED position during twinkle mode
  bool twinkleChange; // Flag set in timer interrupt so the twinkle mode will advance
} twinkleParams_t;

// Structure used to keep runtime parameters of lightning mode
typedef struct {
  state prevMode; // Previous light mode
  state currMode; // Current light mode
  bool modeChange; // Flag set in interrupt routine so a new command will be decoded in loop
} lightMode_t;

#endif // _CONSTANTS_AND_TYPES_H
