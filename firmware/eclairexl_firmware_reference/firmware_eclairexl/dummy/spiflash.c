#include "spiflash.h"

void readFlashId(int *id1, int *id2)
{
	*id1 = 0;
	*id2 = 0;
}

void readFlash(int address, int bytes, u08 * dest)
{
}

void waitWriteComplete()
{
}

int flashSectorSize()
{
	return 0;
}

void eraseFlash(int address, int bytes)
{
}

void writeFlash(int address, int totalbytes, u08 * dest)
{
}


void init_romstart()
{
}

