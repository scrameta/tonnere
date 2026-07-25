#ifndef SPIFLASH_H
#define SPIFLASH_H

#include "integer.h"

int flashSectorSize();
void readFlashId(int * id1, int * id2);
void readFlash(int address, int bytes, u08 * dest);
void eraseFlash(int address, int bytes); // erase enough sectors to clear this many bytes (rounds up to 256KB blocks for EPCS128...)
void writeFlash(int address, int bytes, u08 * dest); // must erase first

#endif

