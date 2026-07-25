#include <alloca.h>
#include <sys/types.h>
#include "integer.h"
#include "regs.h"
#include "pause.h"
#include "printf.h"
#include "joystick.h"
#include "freeze.h"
#include "vidi2c.h"

#include "simpledir.h"
#include "simplefile.h"
#include "fileselector.h"
#include "cartridge.h"
#include "spibase.h"
#include "rom_location.h"

#include "menu.h"

#ifdef LINUX_BUILD
#include "curses_screen.h"
#define after_set_reg_hook() display_out_regs()
#else
#define after_set_reg_hook() do { } while(0)
#endif

#ifdef USB
#include "usb.h"
#include "usb/debug.h"
#define USBSETTINGS
#endif

#include "memory.h"

void test_ram();

extern char ROM_DIR[];
extern unsigned char freezer_rom_present;
extern unsigned char sd_present;

void mainmenu();

// TODO - needs serious cleanup!

// FUNCTIONS in here
// i) pff init - NOT USED EVERYWHERE
// ii) file selector - kind of crap, no fine scrolling - NOT USED EVERYWHERE
// iii) cold reset atari (clears base ram...)
// iv) start atari (begins paused)
// v) freeze/resume atari - NOT USED EVERYWHERE!
// vi) menu for various options - NOT USED EVERYWHERE!
// vii) pause - TODO - base this on pokey clock...

// standard ZPU IN/OUT use...
// OUT1 - 6502 settings (pause,reset,speed)
// pause_n: bit 0 
// reset_n: bit 1
// turbo: bit 2-4: meaning... 0=1.79Mhz,1=3.58MHz,2=7.16MHz,3=14.32MHz,4=28.64MHz,5=57.28MHz,etc.
// ram_select: bit 5-7: 
//   		RAM_SELECT : in std_logic_vector(2 downto 0); -- 64K,128K,320KB Compy, 320KB Rambo, 576K Compy, 576K Rambo, 1088K, 4MB

#define BIT_REG(op,mask,shift,name,reg) \
int get_ ## name() \
{ \
	int val = *reg; \
	return op((val>>shift)&mask); \
} \
void set_ ## name(int param) \
{ \
	int val = *reg; \
	 \
	val = (val&~(mask<<shift)); \
	val |= op(param)<<shift; \
	 \
	*reg = val; \
	after_set_reg_hook(); \
}

#define BIT_REG_RO(op,mask,shift,name,reg) \
int get_ ## name() \
{ \
	int val = *reg; \
	return op((val>>shift)&mask); \
}

BIT_REG(,0x1,0,pause_6502,zpu_out1)
BIT_REG(,0x1,1,reset_6502,zpu_out1)
BIT_REG(,0x3f,2,turbo_6502,zpu_out1)
BIT_REG(,0x7,8,ram_select,zpu_out1)
BIT_REG(,0x1,11,atari800mode,zpu_out1)
//BIT_REG(,0x3f,11,rom_select,zpu_out1)
BIT_REG(,0x3f,17,cart_select,zpu_out1)
// reserve 2 bits for extending cart_select
BIT_REG(,0x01,25,freezer_enable,zpu_out1)
BIT_REG(,0x03,26,key_type,zpu_out1) // ansi,iso,custom1,custom2
#ifndef MIST
BIT_REG(,0x07,28,turbo_drive,zpu_out1) 
#endif
BIT_REG(,0x01,31,turbo_6502_vblank_only,zpu_out1) 

BIT_REG(,0x07,0,video,zpu_out6)
BIT_REG(,0x01,4,tv,zpu_out6)
BIT_REG(,0x01,5,scanlines,zpu_out6)
BIT_REG(,0x01,6,csync,zpu_out6)

#ifdef DEBUG_SUPPORT
BIT_REG(,0xffff,0,debug_addr,zpu_out7)
BIT_REG(,0xff,16,debug_data,zpu_out7)
BIT_REG(,0x1,24,debug_read_mode,zpu_out7)
BIT_REG(,0x1,25,debug_write_mode,zpu_out7)
BIT_REG(,0x1,26,debug_data_match,zpu_out7)
#endif

BIT_REG_RO(,0x1,8,hotkey_softboot,zpu_in1)
BIT_REG_RO(,0x1,9,hotkey_coldboot,zpu_in1)
BIT_REG_RO(,0x1,10,hotkey_settings,zpu_in1)
BIT_REG_RO(,0x1,11,hotkey_fileselect,zpu_in1)

BIT_REG_RO(,0x3f,12,controls,zpu_in1) // (esc)FLRDU
BIT_REG_RO(,0x1,18,sd_detect,zpu_in1) // sd_detect
BIT_REG_RO(,0x1,19,sd_writeprotect,zpu_in1) // sd_writeprotect

//MiST file IO
#ifdef MIST
BIT_REG_RO(,0x07,26,turbo_drive,zpu_in1)
BIT_REG_RO(,0x1,29,sd_ack,zpu_in1)
BIT_REG_RO(,0x3,30,sd_mounted,zpu_in1)

BIT_REG(,0x3,30,sd_read,zpu_out3)
BIT_REG(,0x3,28,sd_write,zpu_out3)
BIT_REG(,0x1,18,sd_secbuf_rd,zpu_out3)
BIT_REG(,0x1,17,sd_secbuf_we,zpu_out3)
BIT_REG(,0x1ff,8,sd_secbuf_addr,zpu_out3)
BIT_REG(,0xff,0,sd_secbuf_d,zpu_out3)
#endif

#define VIDEO_RGB 0
#define VIDEO_SCANDOUBLE 1
#define VIDEO_SVIDEO 2
#define VIDEO_HDMI 3
#define VIDEO_DVI 4
#define VIDEO_VGA 5
#define VIDEO_COMPOSITE 6

#define TV_NTSC 0
#define TV_PAL 1


void
wait_us(int unsigned num)
{
	// pause counter runs in us
	*zpu_pause = num;
#ifdef LINUX_BUILD
	usleep(num);
#endif
#ifdef SOCKIT
	usleep(num);
#endif
}

void memset8(void * address, int value, int length)
{
	char * mem = address;
	while (length--)
		*mem++=value;
}

void memset32(void * address, int value, int length)
{
	int * mem = address;
	while (length--)
		*mem++=value;
}

void clear_main_ram(int clear_cart)
{
	memset8(SRAM_BASE, 0, 1024); // SRAM, if present (TODO)
	memset32(SDRAM_BASE, 0, 1024/4);
	if (clear_cart)
		*atari_cartswitch = 0;
}

void
reboot(int cold)
{
	set_pause_6502(1);
	if (cold)
	{
		set_freezer_enable(0);
		clear_main_ram(cold==1);
		set_freezer_enable(freezer_rom_present);
	}
	set_reset_6502(1);
	// Do nothing in here - this resets the memory controller!
	set_reset_6502(0);
	set_pause_6502(0);
}

unsigned char toatarichar(int val)
{
	int inv = val>=128;
	if (inv)
	{
		val-=128;
	}
	if (val>='A' && val<='Z')
	{
		val+=-'A'+33;
	}
	else if (val>='a' && val<='z')
	{
		val+=-'a'+33+64;
	}
	else if (val>='0' && val<='9')
	{
		val+=-'0'+16;	
	}
	else if (val>=32 && val<=47)
	{
		val+=-32;
	}
	else if (val == ':')
	{
		val = 26;
	}
	else
	{
		val = 0;
	}
	if (inv)
	{
		val+=128;
	}
	return val;
}

int debug_pos;
int debug_adjust;
unsigned char volatile * baseaddr;

#ifdef LINUX_BUILD
void char_out(void* p, char c)
{
	// get rid of unused parameter p warning
	(void)(p);
	int x, y;
	x = debug_pos % 40;
	y = debug_pos / 40;
	display_char(x, y, c, debug_adjust == 128);
	debug_pos++;
}

#else
void clearscreen()
{
	unsigned volatile char * screen;
	for (screen=(unsigned volatile char *)(screen_address+atari_regbase); screen!=(unsigned volatile char *)(atari_regbase+screen_address+1024); ++screen)
		*screen = 0x00;
}

void char_out ( void* p, char c)
{
	unsigned char val = toatarichar(c);
	if (debug_pos>=0)
	{
		*(baseaddr+debug_pos) = val|debug_adjust;
		++debug_pos;
	}
}
#endif

#define NUM_FILES 8
struct SimpleFile * files[NUM_FILES];

void loadromfile(struct SimpleFile * file, int size, unsigned char * ram_address)
{
	void* absolute_ram_address = ram_address;
	int read = 0;
	file_read(file, absolute_ram_address, size, &read);
}

void loadrom(char const * path, int size, unsigned char * ram_address)
{
	if (SimpleFile_OK == file_open_name(path, files[5]))
	{
		loadromfile(files[5], size, ram_address);
	}
}

void loadrom_indir(struct SimpleDirEntry * entries, char const * filename, int size, unsigned char * ram_address)
{
	if (SimpleFile_OK == file_open_name_in_dir(entries, filename, files[5]))
	{
		loadromfile(files[5], size, ram_address);
	}
}

#ifdef LINUX_BUILD
int zpu_main(void)
#else
int main(void)
#endif
{
	INIT_MEM

	init_printf(0, char_out);

#ifdef PLL_SUPPORT
	set_pll2();
#endif
	//set_vidi2c();

	fil_type_rom = "ROM";
	fil_type_bin = "BIN";
	fil_type_car = "CAR";
	fil_type_mem = "MEM";
	fil_type_rpd = "RPD";

	int i;
	for (i=0; i!=NUM_FILES; ++i)
	{
		files[i] = (struct SimpleFile *)alloca(file_struct_size());
		file_init(files[i]);
	}

	freeze_init((void*)FREEZE_MEM); // 128k

	debug_pos = -1;
	debug_adjust = 0;
	baseaddr = (unsigned char volatile *)(screen_address + atari_regbase);
	set_pause_6502(1);
	set_reset_6502(1);
	set_reset_6502(0);
	set_turbo_6502(1);
	set_ram_select(2);
	set_cart_select(0);
	set_freezer_enable(0);
	set_key_type(0);
#ifndef MIST
	set_turbo_drive(0);
#endif
	set_atari800mode(0);

	*atari_colbk = 0xc8;
		//test_ram();

	mainmenu();
	return 0;
}

#ifdef USBSETTINGS
void rotate_usb_sticks()
{
	int max_jindex = hid_get_joysticks()-1;
	if (max_jindex == 0) // If only one stick connected allow it to be 0 or 1
	{
		max_jindex = 1;
	}

	int i;

	usb_device_t * devices = usb_get_devices();
	for (i=0;i!=USB_NUMDEVICES;++i)	
	{
		usb_device_t *dev  = devices + i;
		if (dev->bAddress)
		{
			if (dev->class == &usb_hid_class)
			{
				int j=0;
				for (j=0;j!=dev->hid_info.bNumIfaces;++j)
				{
					int type = dev->hid_info.iface[j].device_type;
					if (type == HID_DEVICE_JOYSTICK)
					{
						int jindex = dev->hid_info.iface[j].jindex;
						event_digital_joystick(jindex, 0);
						event_analog_joystick(jindex, 0,0);

						jindex++;
						if (jindex > max_jindex)
							jindex = 0;
						
						dev->hid_info.iface[j].jindex = jindex;
					}
				}
			}
		}
	}
}

void usb_devices(int debugPos)
{
	usb_device_t *devices = usb_get_devices();
	int nextDebugPos = debugPos;
	int i=0;
	int j=0;
	for (i=0;i!=USB_NUMDEVICES;++i)	
	{
		debug_pos = nextDebugPos;
		usb_device_t *dev  = devices + i;
		if (dev->bAddress)
		{
			if (dev->class == &usb_hub_class)
			{
				//printf("%x.Hub. %d ports. poll=%d",dev->bAddress,dev->hub_info.bNbrPorts, dev->hub_info.bPollEnable);
				printf("%x.Hub. %d ports",dev->bAddress,dev->hub_info.bNbrPorts);
			}
			else if (dev->class == &usb_hid_class)
			{
				//printf("%x.HID. %d ifaces. poll=%d",dev->bAddress,dev->hid_info.bNumIfaces,dev->hid_info.bPollEnable);
				printf("%x.HID",dev->bAddress);
				int adjPos = 0;
				for (j=0;j!=dev->hid_info.bNumIfaces;++j)
				{
					//usb_device_descriptor_t desc;
					int type = dev->hid_info.iface[j].device_type;
					if (adjPos && (type == HID_DEVICE_MOUSE || type == HID_DEVICE_KEYBOARD || type == HID_DEVICE_JOYSTICK))
					{
						nextDebugPos = nextDebugPos+40;
						debug_pos = nextDebugPos;
					}
					if (type == HID_DEVICE_MOUSE)
						printf(" Mouse");
					else if (type == HID_DEVICE_KEYBOARD)
						printf(" Keyboard");
					else if (type == HID_DEVICE_JOYSTICK)
					{
						printf(" Joystick:%d",dev->hid_info.iface[j].jindex+1);
					}
					else
						continue;

					adjPos = 1;

					/*int rcode = usb_get_dev_descr( dev, 12, &desc );
					if( !rcode ) {
						printf(" V:%02x%02x P:%02x%02x C:%02x",desc.idVendorH,desc.idVendorL,desc.idProductH,desc.idProductL,desc.bDeviceClass);
					}*/
				}
			}
		}
		nextDebugPos = nextDebugPos+40;
	}
}
#endif

char const * get_video_mode(int video_mode)
{
	static char const * videotxt[] = 
	{
		"RGB",
		"SCANDOUBLE",
		"SVIDEO",
		"HDMI",
		"DVI",
		"VGA",
		"COMPOSITE"
	};
	return videotxt[video_mode];
}

char const * get_tv_standard(int tv)
{
	static char const * tvtxt[] = 
	{
		"NTSC",
		"PAL"
	};
	return tvtxt[tv];
}

void load_settings(int profile)
{
	struct SimpleDirEntry * entries = dir_entries(ROM_DIR);
	unsigned int settings[2],origSettings[2];
	settings[0] = *zpu_out1;
	settings[1] = *zpu_out6;
	origSettings[0] = settings[0];
	origSettings[1] = settings[1];
#ifndef NO_FLASH
	if (profile==4)
	{
#endif
		if (sd_present && SimpleFile_OK == file_open_name_in_dir(entries, "settings", files[6]))
		{
			int read = 0;
			file_read(files[6], &settings[0], 8, &read);
		}
		else
		{
			return;
		}
#ifndef NO_FLASH
	}
	else
	{
		readFlash(romstart + (profile<<13),8,&settings[0]);
	}
#endif
		
	unsigned int mask = 1|(1<<1)|(0x3f<<17)|(1<<25); //do not override pause, reset, cart_select, freezer_enable
	settings[0] &= ~mask;
	settings[0] |= origSettings[0]&mask;
		
	*zpu_out1 = settings[0];
	*zpu_out6 = settings[1];
		
#ifdef PLL_SUPPORT
	set_pll(get_tv()==TV_PAL, get_video()>=VIDEO_HDMI && get_video()<VIDEO_COMPOSITE);
#endif
}

void save_settings(int profile)
{
	struct SimpleDirEntry * entries = dir_entries(ROM_DIR);
	unsigned int settings[2];
	settings[0] = *zpu_out1;
	settings[1] = *zpu_out6;

#ifndef NO_FLASH
	if (profile==4)
	{
#endif
		if (sd_present && SimpleFile_OK == file_open_name_in_dir(entries, "settings", files[6]))
		{
			file_seek(files[6],0);
			int written = 0;
			file_write(files[6],&settings[0],8,&written);
			file_write_flush();
		}
#ifndef NO_FLASH
	}
	else
	{
		int size = 192; // last 64KB is unused

		clearscreen();
		debug_pos = 0;
		debug_adjust = 0;
		printf("Flashing settings");

		debug_pos = 80;

		printf("DO NOT POWER OFF");
	
		debug_pos = 160;


		printf("Read     ");
		readFlash(romstart,size*1024,SCRATCH_MEM); // back up rest of settings
		printf("Erase    ");
		eraseFlash(romstart,size*1024); // Clear at last 256kB
		printf("Write    ");

		memcp8(&settings[0],SCRATCH_MEM+(profile<<13),0,8);
		memcp8(os_addr(),SCRATCH_MEM+0x10000+(profile<<14),0,16384);
		memcp8(basic_addr(),SCRATCH_MEM+0x8000+(profile<<13),0,8192);

		int romstartsectoraligned = romstart&0xffffff00;
		int aligncorrection = romstart-romstartsectoraligned;
		int extra = 0;
		if (aligncorrection!=0)
		{
			extra = 256;
		}
		writeFlash(romstartsectoraligned,size*1024+extra,SCRATCH_MEM-aligncorrection);// write back rest of settings*/
		printf("Done    ");
		wait_us(1000000);
	}
#endif
}


