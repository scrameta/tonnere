#include "regs.h"

char native_porta;
char native_trig;

unsigned char volatile * atari_porta = (unsigned char volatile *)&native_porta;
unsigned char volatile * atari_trig0 = (unsigned char volatile *)&native_trig;

