#include "memory.h"

unsigned char * basic_addr()
{
	return (ROM_BASE + 0xc000);
}

unsigned char * os_addr()
{
	return (ROM_BASE + 0x4000);
}

