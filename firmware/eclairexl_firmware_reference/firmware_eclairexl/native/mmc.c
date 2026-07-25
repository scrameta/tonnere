#include "mmc.h"

#include <stdio.h>

FILE * disk_image;
char mmc_sector_buffer[512];

void set_spi_clock_freq()
{
}

//! Initialize AVR<->MMC hardware interface.
/// Prepares hardware for MMC access.

void mmc_init()
{
	mmcInit();
}

void mmcInit(void)
{
	//disk_image = fopen("/home/markw/fpga/sd_images/sd.image","r+");
	//disk_image = fopen("/home/markw/fpga/sd_images/sd_large.image","r+");
	disk_image = fopen("/home/markw/fpga/sd_images/sdimage_elektraglide","r+");

	fprintf(stderr,"mmcInit:%x\n",disk_image);
}

//! Initialize the card and prepare it for use.
/// Returns zero if successful.
u08 mmcReset(void){ return 0;}

//! Read 512-byte sector from card to buffer
/// Returns zero if successful.
u08 mmcRead(u32 sector)
{
	//fprintf(stderr,"mmcRead:%x\n",sector);

	fseek(disk_image, sector*512, SEEK_SET);
	fread(&mmc_sector_buffer,512,1,disk_image);

	return 0;
}

//! Write 512-byte sector from buffer to card
/// Returns zero if successful.
u08 mmcWrite(u32 sector)
{
	fprintf(stderr,"mmcWrite:%x\n",sector);

	fseek(disk_image, sector*512, SEEK_SET);
	fwrite(&mmc_sector_buffer,512,1,disk_image);

	return 0;
}

