#ifndef _MUSIC_MODE_HPP
#define _MUSIC_MODE_HPP

#include "MusicMode.h"

/*************************************************************************************************\
 *                                  Functions for music mode                                     *
\*************************************************************************************************/

void enable_ADC() {
  // Enable ADC
  ADCSRA |= (1 << ADEN);
}

void disable_ADC() {
  // Disable ADC
  ADCSRA &= ~(1 << ADEN);
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

#endif // _MUSIC_MODE_HPP