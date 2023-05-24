#ifndef _ISRS_TIMERS_ADC_HPP
#define _ISRS_TIMERS_ADC_HPP

#include "ISRsTimersADC.h"

/*************************************************************************************************\
 *                  Functions for interrupt routines, timer setups and ADC                       *
\*************************************************************************************************/

// Callback after ISR routine on IR_PIN is over
void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags) {
  // Save received command
  command = aCommand;
  // Set flag to decode it
  lightMode.modeChange = true;
}

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

#endif // _ISRS_TIMERS_ADC_HPP