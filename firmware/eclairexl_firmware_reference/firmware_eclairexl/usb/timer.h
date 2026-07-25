// interface between USB timer and minimig timer

#ifndef TIMER_H
#define TIMER_H

//#include <inttypes.h>
#include <common/integer.h>
typedef uint32_t msec_t;

void timer_init();
msec_t timer_get_msec();

bool timer_elapsed(msec_t end);

#endif // TIMER_H
