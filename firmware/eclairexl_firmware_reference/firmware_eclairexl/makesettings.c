#include <stdio.h>

#define VIDEO_RGB 0
#define VIDEO_SCANDOUBLE 1
#define VIDEO_SVIDEO 2
#define VIDEO_HDMI 3
#define VIDEO_DVI 4
#define VIDEO_VGA 5

#define TV_NTSC 0
#define TV_PAL 1


// turbo: bit 2-4: meaning... 1=1x... etc.  (old...0=1.79Mhz,1=3.58MHz,2=7.16MHz,3=14.32MHz,4=28.64MHz,5=57.28MHz,etc.
// ram_select: bit 5-7: 
//   		RAM_SELECT : in std_logic_vector(2 downto 0); -- 64K,128K,320KB Compy, 320KB Rambo, 576K Compy, 576K Rambo, 1088K, 4MB

#define ByteSwap32(n) \
    ( ((((unsigned long) n) << 24) & 0xFF000000) |    \
      ((((unsigned long) n) <<  8) & 0x00FF0000) |    \
      ((((unsigned long) n) >>  8) & 0x0000FF00) |    \
      ((((unsigned long) n) >> 24) & 0x000000FF) )

unsigned int settings[2];
void save(const char * filename)
{
	settings[0] = ByteSwap32(settings[0]);
	settings[1] = ByteSwap32(settings[1]);
	FILE * fd;
	fd = fopen(filename,"w");
	fwrite(&settings[0],8,1,fd);
	fclose(fd);
}

int main(void)
{
	int mem64k = 0<<8;
	int mem128k = 1<<8;
	int mem320kcompy = 2<<8;
	int mem320krambo = 3<<8;
	int mem576kcompy = 4<<8;
	int mem576krambo = 5<<8;
	int mem1MB = 6<<8;
	//int mem4MB = 7<<8;
	//
	int mem8k = 0<<8;
	int mem16k = 1<<8;
	int mem32k = 2<<8;
	int mem48k = 3<<8;
	int mem52k = 4<<8;
	//
	int xlxe = 0<<11;
	int atari800 = 1<<11;

	int speed1x = 1<<2;
	int speed2x = 2<<2;
	int speed4x = 4<<2;
	int speed8x = 8<<2;
	int speed16x = 16<<2;
	//int speed32x = 32<<2;

	int scanlines = 1<<5;
	int csync = 1<<6;
	int pal = TV_PAL<<4;
	int ntsc = TV_NTSC<<4;


	settings[0] = xlxe|mem64k|speed1x; //64KB
	settings[1] = VIDEO_RGB | pal | scanlines | csync;
	save("64k_PAL_RGB");

	settings[0] = xlxe|mem128k|speed1x; //128KB
	settings[1] = VIDEO_SCANDOUBLE | ntsc | scanlines | csync;
	save("128k_NTSC_SCANDOUBLE");

	settings[0] = xlxe|mem576kcompy|speed1x; //128KB
	settings[1] = VIDEO_VGA | pal | csync;
	save("576kcompy_PAL_VGA");

	settings[0] = xlxe|mem576kcompy|speed1x; //128KB
	settings[1] = VIDEO_HDMI | pal | csync;
	save("576kcompy_PAL_HDMI");

	settings[0] = xlxe|mem576kcompy|speed1x; //128KB
	settings[1] = VIDEO_DVI | pal | csync;
	save("576kcompy_PAL_DVI");

	settings[0] = xlxe|mem576kcompy|speed1x; //128KB
	settings[1] = VIDEO_HDMI | ntsc | csync;
	save("576kcompy_NTSC_HDMI");

	settings[0] = xlxe|mem576kcompy|speed1x; //128KB
	settings[1] = VIDEO_DVI | ntsc | csync;
	save("576kcompy_NTSC_DVI");

	settings[0] = atari800|mem48k|speed1x; //48k 800
	settings[1] = VIDEO_VGA | pal | csync;
	save("48k800_PAL_VGA");


	settings[0] = xlxe|mem576kcompy|speed1x; //128KB
	settings[1] = VIDEO_VGA | pal;
	save("576kcompy_PAL_VGA_NCS");

	settings[0] = atari800|mem48k|speed1x; //48k 800
	settings[1] = VIDEO_VGA | pal;
	save("48k800_PAL_VGA_NCS");

	settings[0] = xlxe|mem576kcompy|speed1x; //128KB
	settings[1] = VIDEO_VGA;
	save("576kcompy_NTSC_VGA_NCS");

	settings[0] = atari800|mem48k|speed1x; //48k 800
	settings[1] = VIDEO_VGA;
	save("48k800_NTSC_VGA_NCS");

	return 0;
}


