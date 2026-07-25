#include <stdint.h>
#include <string.h>
#include "regs.h"
#include "memory.h"
#include "linux_memory.h"

void* ROM_BASE;
void* SRAM_BASE;
void* SDRAM_BASE;
void* atari_regbase;
void* atari_regmirror;
void* zpu_regbase;
void* pokey_regbase;
void* pll_regbase;
void* CARTRIDGE_MEM;

void* FREEZER_RAM_MEM;
void* FREEZER_ROM_MEM;
void* SCRATCH_MEM;

#define ROM_SIZE (32*1024)
#define SRAM_SIZE (512*1024)
#define SDRAM_SIZE (8*1024*1024)
#define ATARI_SIZE (64*1024)
#define CONFIG_SIZE (256)
#define CARTRIDGE_SIZE (2*1024*1024)
#define FREEZER_RAM_SIZE (128*1024)
#define FREEZER_ROM_SIZE (64*1024)

uint8_t rom_memory[ROM_SIZE];
uint8_t sram_memory[SRAM_SIZE];
uint32_t sdram_memory[SDRAM_SIZE / 4];
uint8_t atari_memory[ATARI_SIZE];
uint8_t atari_mirror_memory[ATARI_SIZE];
uint32_t config_memory[CONFIG_SIZE/4];
uint32_t cartridge_memory[CARTRIDGE_SIZE / 4];

uint32_t freezer_ram_memory[FREEZER_RAM_SIZE / 4];
uint32_t freezer_rom_memory[FREEZER_ROM_SIZE / 4];

void init_memory(void)
{
	memset(rom_memory, ROM_SIZE, 0);
	memset(sram_memory, SRAM_SIZE, 0);
	memset(sdram_memory, SDRAM_SIZE, 0);
	memset(atari_memory, ATARI_SIZE, 0);
	memset(atari_mirror_memory, ATARI_SIZE, 0);
	memset(config_memory, CONFIG_SIZE, 0);
	memset(cartridge_memory, CARTRIDGE_SIZE, 0);
	memset(freezer_ram_memory, FREEZER_RAM_SIZE, 0);
	memset(freezer_rom_memory, FREEZER_ROM_SIZE, 0);

	ROM_BASE = rom_memory;
	SRAM_BASE = sram_memory;
	SDRAM_BASE = sdram_memory;
	atari_regbase = atari_memory;
	atari_regmirror = atari_mirror_memory;
	zpu_regbase = config_memory;
	pokey_regbase = config_memory+0x100*4;
	pll_regbase = config_memory+0x200*4;
	CARTRIDGE_MEM = cartridge_memory;
	FREEZER_RAM_MEM = freezer_ram_memory;
	FREEZER_ROM_MEM = freezer_rom_memory;
	SCRATCH_MEM = freezer_rom_memory;

	// command line is high
	*zpu_sio = 1;
	// no trigger pressed
	*atari_trig0 = 1;
}

