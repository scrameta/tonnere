#include "spiflash.h"
#include "spi.h"

#include "regs.h"

void readFlashId(int *id1, int *id2)
{
	int i=0;
	int bytes = 4;
	unsigned char * id1c = (unsigned char *)id1;
	unsigned char * id2c = (unsigned char *)id2;
	unsigned char res;

	flashChipSelect();
	spiTransferByte(0xab);
	for (i=0; i!=bytes; ++i)
	{
		res = spiTransferByte(0xff);
		id1c[i] = res;
	}

	flashChipDeselect();

	flashChipSelect();
	spiTransferByte(0x9f);
	for (i=0; i!=bytes; ++i)
	{
		res = spiTransferByte(0xff);
		id2c[i] = res;
	}

	flashChipDeselect();

/*	id1c[0] = 0x11;
	id1c[1] = 0x22;
	id1c[2] = 0x33;
	id1c[3] = 0x44;
	id2c[0] = 0x55;
	id2c[1] = 0x66;
	id2c[2] = 0x77;
	id2c[3] = 0x88;*/
}

void readFlash(int address, int bytes, u08 * dest)
{
	int i=0;
	u08 res;
	flashChipSelect();
	spiTransferByte(0x03);
	res = spiTransferByte((address>>16)&0xff);
	res = spiTransferByte((address>>8)&0xff);
	res = spiTransferByte(address&0xff);
	for (i=0; i!=bytes; ++i)
	{
		res = spiTransferByte(0xff);
		dest[i] = res;
	}
	flashChipDeselect();
}

void waitWriteComplete()
{
	u08 res;
	while (true)
	{
		flashChipSelect();
		spiTransferByte(0x5); // read status: check if done
		res = spiTransferByte(0xff);
		flashChipDeselect();
		if (0==res&0x1)
		{
			break;
		}
	}
}

int flashSectorSize()
{
	int sectorSize; /* Size for D8 erase... */

	unsigned int id1,id2;
	readFlashId(&id1,&id2);
	id2 = id2>>8;
	if (id2==0x20ba18)
	{ 
		sectorSize = 64*1024;
	}
	else if (id2==0x202018)
	{
		sectorSize = 256*1024;
	}
	else if (id2==0x202015)
	{
		sectorSize = 64*1024;
	}
	else if (id2==0x012018)
	{
		sectorSize = 64*1024;
	}
	else if (id2==0x1c7018)
	{
		sectorSize = 64*1024;
	}
	else
	{
		sectorSize = -1;
	}
	return sectorSize;
}

void eraseFlash(int address, int bytes)
{
	u08 res;

	int sectorSize = flashSectorSize();
	if (sectorSize<0)
	{
		return;
	}

	while (bytes>0)
	{
		flashChipSelect();
		spiTransferByte(0x6); // write enable
		flashChipDeselect();
		
		flashChipSelect();
		spiTransferByte(0xd8); // erase sector
		res = spiTransferByte((address>>16)&0xff);
		res = spiTransferByte((address>>8)&0xff);
		res = spiTransferByte(address&0xff);

		flashChipDeselect();

		bytes = bytes-sectorSize;
		address = address+sectorSize;

		waitWriteComplete();
	}
}

void writeFlash(int address, int totalbytes, u08 * dest)
{
	u08 res;
	while(totalbytes>0)
	{
		int bytes = totalbytes;
		int i;
		if (bytes>256)
			bytes=256; // Have to write in chunks of 256 bytes

		flashChipSelect();
		spiTransferByte(0x6); // write enable
		flashChipDeselect();
	
		flashChipSelect();
		spiTransferByte(0x2); // write bytes
		res = spiTransferByte((address>>16)&0xff);
		res = spiTransferByte((address>>8)&0xff);
		res = spiTransferByte(address&0xff);
		for (i=0; i!=bytes; ++i)
		{
			spiTransferByte(dest[i]);
		}
		flashChipDeselect();

		totalbytes-=bytes;
		address+=bytes;
		dest+=bytes;
	
		waitWriteComplete();
	}
}


