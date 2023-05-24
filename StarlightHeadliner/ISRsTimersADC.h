#ifndef _ISRS_TIMERS_ADC_H
#define _ISRS_TIMERS_ADC_H

#include "ConstantsAndTypes.h"

/*************************************************************************************************\
 *                                      Global Variables                                         *
\*************************************************************************************************/

extern volatile stripParams_t wideStripParams;
extern volatile stripParams_t narrowStripParams;
extern volatile twinkleParams_t twinkleParams;
extern volatile bool brightnessChanged;
extern volatile uint8_t command;
extern lightMode_t lightMode;

/*************************************************************************************************\
 *                                     Function prototypes                                       *
\*************************************************************************************************/
ISR(ADC_vect);
ISR(TIMER1_COMPA_vect);
ISR(TIMER1_COMPB_vect);
void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags);
void setup_ADC();
void setup_timer1();

#endif // _ISRS_TIMERS_ADC_H
