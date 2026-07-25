#include "memory.h"
#include "regs.h"

void clearscreen()
{
/*	unsigned volatile char * screen;
	for (screen=(unsigned volatile char *)(screen_address+atari_regbase); screen!=(unsigned volatile char *)(atari_regbase+screen_address+1024); ++screen)
		*screen = 0x00;*/
}

#include "stdlib.h"
#include "stdio.h"

void memset8(void * address, int value, int length)
{
	char * mem = address;
	while (length--)
		*mem++=value;
}

#undef atari_regbase
static char * atari_regbase = (char *)malloc(65536);
#undef atari_regmirror
static char * atari_regmirror = (char *)malloc(65536);
#include "freeze.h"
#include "a800/freeze_ci.c"
#include "regs.c"

int main(void)
{
	char * buffer = (char *)malloc(1024*128);
	freeze_init(buffer);

	for (int i=0; i!=64*1024; ++i)
	{
		*((unsigned char *)(atari_regbase+i)) = i%256;
	}
	
	for (int i=0;i!=0x10000;++i)
	{
		*((atari_regmirror+i)) = i%256-2;
	}

	freeze();

	for (int i=0; i!=64*1024; ++i)
	{
		fprintf(stderr,"i:%x stored:%x regbase:%x mirror:%x\n",i,
			*((unsigned char *)(buffer+i)),
			*((unsigned char *)(atari_regbase+i)),
			*((unsigned char *)(atari_regmirror+i))
		);
/*		if (i%256!=*((unsigned char *)(buffer+i)))
		{
			fprintf(stderr,"!!FAIL!%d %d %d", i, i%256, *(unsigned char *)(atari_regbase+i));
			return -1;
		}*/
	}
	return 0;

	for (int i=0;i!=0xd000; ++i)
	{
		*((atari_regbase+i)) = 0;
	}
	for (int i=0xd800;i!=0x10000; ++i)
	{
		*((atari_regbase+i)) = 0;
	}
	restore();

	for (int i=0; i!=64*1024; ++i)
	{
		unsigned char exp = i%256;
		if (i>=0xd000 && i<0xd800) exp-=2;
		if (i>=0xd020 && i<0xd200) exp+=2;
		if (i>=0xd220 && i<0xd400) exp+=2;
		if (i>=0xd410 && i<0xd800) exp+=2;
		if (exp!=*((unsigned char *)(atari_regbase+i)))
		{
			fprintf(stderr,"FAIL!addr:%x exp:%d got:%d", i, exp, *(unsigned char *)(atari_regbase+i));
			return -1;
		}
	}
	return 0;
}

