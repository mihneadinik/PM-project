#ifndef _LIGHT_MODE_HPP
#define _LIGHT_MODE_HPP

#include "LightMode.h"

/*************************************************************************************************\
 *                                  Functions for light modes                                    *
\*************************************************************************************************/

// Stops timer counting channel for twkinkle changes
void stop_twinkle_timer() {
  cli();

  // Disable channel A interrupt
  TIMSK1 &= ~(1 << OCIE1A);
  
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

#endif // _LIGHT_MODE_HPP