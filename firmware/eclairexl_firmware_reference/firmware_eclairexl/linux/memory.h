#ifndef MEMORY_H
#define MEMORY_H

extern void* ROM_BASE;
extern void* SDRAM_BASE;
extern void* SRAM_BASE;
extern void* CARTRIDGE_MEM;
extern void* FREEZER_RAM_MEM;
extern void* FREEZER_ROM_MEM;
extern void* SCRATCH_MEM;

#define HAVE_FREEZER_ROM_MEM 1

#define INIT_MEM

// Memory usage...
// 0x410000-0x44FFFF (0xc10000 in zpu space) = directory cache - 256k
// 0x450000-0x46FFFF (0xc50000 in zpu space) = freeze backup
// 0x700000-0x77FFFF (0xf00000 in zpu space) = os rom/basic rom

#define DIR_INIT_MEM (SDRAM_BASE + 0x410000)
#define DIR_INIT_MEMSIZE 262144
#define FREEZE_MEM (SDRAM_BASE + 0x450000)

// offset into SDRAM
#define ROM_OFS 0x700000

extern void* atari_regbase;
extern void* atari_regmirror;
extern void* zpu_regbase;
extern void* pokey_regbase;
extern void* pll_regbase;

void init_memory(void);

#endif
