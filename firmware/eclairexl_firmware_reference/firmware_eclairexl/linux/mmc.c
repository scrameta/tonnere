#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mmc.h"
#include "curses_screen.h"
#include "linux_helper.h"

#define LOG(x...) print_log(x)

FILE * disk_image = NULL;

char mmc_sector_buffer[512];

void set_spi_clock_freq()
{
}

//! Initialize AVR<->MMC hardware interface.
/// Prepares hardware for MMC access.

void mmcInit(void)
{
	if (!sdcard_filename) {
		LOG("no sdcard\n");
		return;
	}
	disk_image = fopen(sdcard_filename,"r+");
	if (!disk_image) {
		LOG("cannot open %s\n", sdcard_filename);
		perror(0);
	} else {
		LOG("opened sdcard %s\n", sdcard_filename);
	}
}

void mmc_init()
{
	mmcInit();
}


//! Initialize the card and prepare it for use.
/// Returns zero if successful.
u08 mmcReset(void){ return 0;}

//! Read 512-byte sector from card to buffer
/// Returns zero if successful.
u08 mmcRead(u32 sector)
{
	if (!disk_image) {
		goto no_data;
	}
	//fprintf(stderr,"mmcRead:%x\n",sector);

	if (fseek(disk_image, sector*512, SEEK_SET)) {
		LOG("seek to sector %d failed\n", sector);
		goto no_data;
	}
	if (fread(&mmc_sector_buffer,512,1,disk_image) != 1) {
		LOG("reading sector %d failed\n", sector);
		goto no_data;
	}

	return 0;
no_data:
	memset(&mmc_sector_buffer, 0, 512);
	return 0;
}

//! Write 512-byte sector from buffer to card
/// Returns zero if successful.
u08 mmcWrite(u32 sector)
{
	if (!disk_image) {
		goto no_data;
	}
	//fprintf(stderr,"mmcWrite:%x\n",sector);

	if (fseek(disk_image, sector*512, SEEK_SET)) {
		LOG("seek to sector %d failed\n", sector);
		goto no_data;
	}
	if (fwrite(&mmc_sector_buffer,512,1,disk_image) != 1) {
		LOG("reading sector %d failed\n", sector);
		goto no_data;
	}
	return 0;
no_data:
	return 0;
}

