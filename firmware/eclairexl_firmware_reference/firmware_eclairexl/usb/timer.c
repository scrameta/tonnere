#include "timer.h"
#include "regs.h"

// this is a 32 bit counter which overflows after 2^32 milliseconds
// -> after 46 days

void wait_us(int unsigned num);

void timer_init() {
}

#ifdef LINUX_BUILD
#include <sys/time.h>
msec_t timer_get_msec() {
	struct timeval x;
	gettimeofday(&x,0);
	return (x.tv_sec*1000+(x.tv_usec/1000));
}
#else
msec_t timer_get_msec() {
	int res = *zpu_timer;
	res = res >> 10; // Divide by 1024, good enough for here!

	return res;
}

bool timer_elapsed(msec_t until)
{
	msec_t now = timer_get_msec();
	if (now>=until)
		return true;
	else 
	{
		unsigned int delay = until - now;

		if (delay > 0x200000)
			return true;  //overflow
		else
			return false; //normal case
	}
}

#endif

