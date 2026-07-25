#include "spiflash.h"

#include "simplefile.h"
#include "simpledir.h"

extern unsigned char sd_present;
extern char ROM_DIR[];
extern struct SimpleFile * files[];

void readFlashId(int *id1, int *id2)
{
	*id1 = 0x5d;
	*id2 = 0xca7d;
/*	id1c[0] = 0x11;
	id1c[1] = 0x22;
	id1c[2] = 0x33;
	id1c[3] = 0x44;
	id2c[0] = 0x55;
	id2c[1] = 0x66;
	id2c[2] = 0x77;
	id2c[3] = 0x88;*/
}

bool openFlash()
{
	struct SimpleDirEntry * entries = dir_entries(ROM_DIR);
	return (sd_present && SimpleFile_OK == file_open_name_in_dir(entries, "sdflash.bin", files[6]));
}

void readFlash(int address, int bytes, u08 * dest)
{
	if (openFlash())
       	{
		int read = 0;
		file_seek(files[6],address);
               	file_read(files[6], dest, bytes, &read);
	}
}

void waitWriteComplete()
{
	file_write_flush();
}

int flashSectorSize()
{
	return 512;
}

void eraseFlash(int address, int bytes)
{
}

void writeFlash(int address, int totalbytes, u08 * dest)
{
	if (openFlash())
       	{
		int written = 0;
		file_seek(files[6],address);
               	file_write(files[6], dest, totalbytes, &written);
	}
}


