#include "memory.h"

unsigned char * basic_addr()
{
	return (SDRAM_BASE+ROM_OFS);
}

unsigned char * os_addr()
{
	return (SDRAM_BASE+ROM_OFS+0x4000);
}


