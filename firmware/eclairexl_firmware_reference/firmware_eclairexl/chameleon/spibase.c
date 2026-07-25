#include "spibase.h"

#include "regs.h"

void init_romstart()
{
	int i;
	int flashslot = 0;
	int flashslotvalid = 0;
	for (i=0;i!=1000;++i) /* Wait for up to 1 second for flashslot to be valid */
	{
		flashslot = *zpu_in2;
		flashslotvalid = (flashslot&0x10)!=0;
		if (flashslotvalid)
			break;
		wait_us(1000);
	}

	flashslot = (flashslot&0xf)<<20;
	char memtmp[4];
	readFlash(flashslot,3,&memtmp[0]);
	int corelen =  (memtmp[0]<<16)+(memtmp[1]<<8)+(memtmp[2]);
	romstart = flashslot+corelen;
	
	/*uint32_t infolen;
	uint32_t infooff;
	readFlash(romstart-8,4,&infolen);
	readFlash(romstart-4,4,&infooff);
	uint32_t magic;
	readFlash(romstart-infolen,4,&magic);*/
	
	romstart = romstart+262144;
}
