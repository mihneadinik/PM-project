#ifndef _LIGHT_MODE_H
#define _LIGHT_MODE_H

#include "ConstantsAndTypes.h"
#include <Adafruit_NeoPixel.h>

/*************************************************************************************************\
 *                                      Global Variables                                         *
\*************************************************************************************************/

extern Adafruit_NeoPixel pixelsWide;
extern Adafruit_NeoPixel pixelsNarrow;

extern volatile stripParams_t wideStripParams;
extern volatile stripParams_t narrowStripParams;
extern volatile twinkleParams_t twinkleParams;
extern lightMode_t lightMode;

/*************************************************************************************************\
 *                                     Function prototypes                                       *
\*************************************************************************************************/

void stop_twinkle_timer();
void start_twinkle_timer();
uint16_t get_random_color();
void change_brightness(direction dir);
void change_color(direction dir);
void static_mode();
void _execute_twinkle();
void twinkle_mode();
void update_timer_status();

#endif // _LIGHT_MODE_H
