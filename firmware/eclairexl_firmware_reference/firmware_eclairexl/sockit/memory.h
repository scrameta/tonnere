#ifndef MEMORY_H
#define MEMORY_H

#include "stdlib.h"

extern unsigned char volatile * virtual_base;
extern unsigned char volatile * zpu_base;

#define SRAM_BASE ((void*) virtual_base+0x200000)
#define SDRAM_BASE ((void*) virtual_base+0x800000)

// Memory usage...
// 0x410000-0x44FFFF (0xc10000 in zpu space) = directory cache - 256k
// 0x450000-0x46FFFF (0xc50000 in zpu space) = freeze backup
// 0x700000-0x77FFFF (0xf00000 in zpu space) = os rom/basic rom

#define INIT_MEM init_bridge();
void init_bridge();

#define DIR_INIT_MEMSIZE 262144
#define DIR_INIT_MEM (char *)malloc(DIR_INIT_MEMSIZE)
#define FREEZE_MEM (char unsigned *)malloc(128*1024)

#define FREEZER_RAM_MEM (SDRAM_BASE + 0x480000)
#define FREEZER_ROM_MEM (SDRAM_BASE + 0x4A0000)

#define SCRATCH_MEM (SDRAM_BASE + 0x4B0000)
#define CARTRIDGE_MEM (SDRAM_BASE + 0x500000)

// offset into SDRAM
#define ROM_OFS 0x700000

#define atari_regbase  ((void*) virtual_base + 0x10000)
#define atari_regmirror  ((void*) virtual_base + 0x20000)
#define zpu_regbase ((void*) zpu_base)
#define pokey_regbase ((void*) zpu_base + 0x400)

#endif
