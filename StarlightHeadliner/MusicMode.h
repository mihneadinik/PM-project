#ifndef _MUSIC_MODE_H
#define _MUSIC_MODE_H

#include "ConstantsAndTypes.h"

/*************************************************************************************************\
 *                                      Global Variables                                         *
\*************************************************************************************************/

extern volatile stripParams_t wideStripParams;
extern volatile stripParams_t narrowStripParams;
extern lightMode_t lightMode;

/*************************************************************************************************\
 *                                     Function prototypes                                       *
\*************************************************************************************************/

void enable_ADC();
void disable_ADC();
void stop_music_timer();
void start_music_timer();
void update_ADC_status();

#endif // _MUSIC_MODE_H
