#include "main.h" //!!!
#include "atari_drive_emulator.h"
#include "log.h"
#include "utils.h"
#include "sd_direct/spi.h"
#include "rom_location.h"
#include "printf.h"
#include "menu.h"

unsigned char freezer_rom_present;
unsigned char sd_present;
unsigned char sel_profile;
#ifndef NO_FLASH
unsigned int romstart;
#endif

void loadosrom()
{
	if (!sd_present) return;
	if (file_size(files[5]) == 0x4000)
	{
		loadromfile(files[5],0x4000, os_addr());
	}
	else if (file_size(files[5]) ==0x2800)
	{
		loadromfile(files[5],0x2800, os_addr()+0x1800);
		memset8(os_addr(),0xff,0x1000);
	}
	else if (file_size(files[5]) ==0x2000)
	{
		loadromfile(files[5],0x2000, basic_addr());
	}
}

#ifdef USB
struct usb_host usb_porta;
#endif
#ifdef USB2
struct usb_host usb_portb;
#endif

void test_ram()
{
	int i;
	unsigned int volatile * addr = DIR_INIT_MEM;
	int k;
	for (k=0;k<DIR_INIT_MEMSIZE/4;++k)
	{
		addr[k] = k&0xffff;
	}
	int ok = 1;
	for (k=0;k<DIR_INIT_MEMSIZE/4;++k)
	{
		unsigned int val = addr[k];
		if (val != (k&0xffff))
		{
			ok = 0;
		}
	}

	int j =0;
	if (ok)
	{
		while(1)
		{
			++j;
			if (j&1)
				*atari_colbk = 0xc8;
			else
				*atari_colbk = 0x00;
		}
	}
	else
	{
		while(1)
		{
			++j;
			if (j&1)
				*atari_colbk = 0x38;
			else
				*atari_colbk = 0x00;
		}
	}
}

int init_sd()
{
	if (SimpleFile_OK == dir_init((void *)DIR_INIT_MEM, DIR_INIT_MEMSIZE))
	{
		//for(;;);
		#ifdef USB
			usb_log_init(files[7]);
		#endif

		//test_ram();

		return 1;
	}
	return 0;
}

/*static const unsigned char BitReverseTable256[] = 
{
	  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 
	    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 
	      0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 
	        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 
		  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 
		    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
		      0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 
		        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
			  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
			    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 
			      0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
			        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
				  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 
				    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
				      0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 
				        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};*/

#ifdef RPD_SUPPORT
int flash_rpd(struct SimpleFile* file)
{
	int len;
	int readlen;
	int loc;
	int p;

	len = file_size(file);
	loc = 0;

	clearscreen();
	debug_pos = 0;
	debug_adjust = 0;
	printf("Flashing core");

	debug_pos = 80;

	printf("DO NOT POWER OFF");

	debug_pos = 160;

	while (len>0)
	{
		int chunk = len;
		int i = 0;
		if (chunk>262144)
		{
			chunk = 262144;
		}
		len = len-chunk;

		enum SimpleFileStatus ok;
		file_seek(file,loc);
		ok = file_read(file, SCRATCH_MEM, chunk, &readlen);
		if (ok != SimpleFile_OK) {
			LOG("cannot read rpd data\n");
			return 0;
		}

		// Byte swap it!
		for (i=0;i!=chunk;++i)
		{
			unsigned char in = ((unsigned char *)SCRATCH_MEM)[i];
			unsigned char res = 0;
			if (in&1)
				res=res|128;
			if (in&2)
				res=res|64;
			if (in&4)
				res=res|32;
			if (in&8)
				res=res|16;
			if (in&16)
				res=res|8;
			if (in&32)
				res=res|4;
			if (in&64)
				res=res|2;
			if (in&128)
				res=res|1;

			((unsigned char *)SCRATCH_MEM)[i] = res;
		}

		// Flash it!
		//printf("Erase    %x %x",loc,chunk);
		printf(".E.");
		eraseFlash(loc,chunk); // Clear at last 256kB
		//printf("Write    ");
		printf(".W.");
		writeFlash(loc,chunk,SCRATCH_MEM);
		//printf("Verify    ");
		printf(".V.");
		readFlash(loc, chunk, CARTRIDGE_MEM);

		for (i=0;i!=chunk;++i)
		{
			unsigned char in = ((unsigned char *)SCRATCH_MEM)[i];
			unsigned char inb = ((unsigned char *)CARTRIDGE_MEM)[i];
			if (in!=inb)
			{
				printf("FAIL:%02x %02x %d ", in, inb, i);
				break;
			}
		}

		loc = loc+chunk;
	}
	printf("Done    ");
	wait_us(1000000);

/*	printf("   VERIFY  ");
	len = file_size(file);
	loc = 0;
	while (len>0)
	{
		int chunk = len;
		int i = 0;
		if (chunk>262144)
		{
			chunk = 262144;
		}
		len = len-chunk;

		enum SimpleFileStatus ok;
		file_seek(file,loc);
		ok = file_read(file, SCRATCH_MEM, chunk, &readlen);
		if (ok != SimpleFile_OK) {
			LOG("cannot read rpd data\n");
			return 0;
		}

		// Byte swap it!
		for (i=0;i!=chunk;++i)
		{
			unsigned char in = ((unsigned char *)SCRATCH_MEM)[i];
			unsigned char res = 0;
			if (in&1)
				res=res|128;
			if (in&2)
				res=res|64;
			if (in&4)
				res=res|32;
			if (in&8)
				res=res|16;
			if (in&16)
				res=res|8;
			if (in&32)
				res=res|4;
			if (in&64)
				res=res|2;
			if (in&128)
				res=res|1;

			((unsigned char *)SCRATCH_MEM)[i] = res;
		}

		// Flash it!
		printf("Verify    %x %x",loc,chunk);
		readFlash(loc, chunk, CARTRIDGE_MEM);

		for (i=0;i!=chunk;++i)
		{
			unsigned char in = ((unsigned char *)SCRATCH_MEM)[i];
			unsigned char inb = ((unsigned char *)CARTRIDGE_MEM)[i];
			if (in!=inb)
			{
				printf("FAIL:%02x %02x %d ", in, inb, i);
				break;
			}
		}

		loc = loc+chunk;
	}

	debug_pos=400;
	printf("HERE");

	//file_seek(file,0);
	//file_read(file, SCRATCH_MEM, 256, &readlen);
	readFlash(0, 256, SCRATCH_MEM);
	for (p=0;p!=256;++p)
	{
		unsigned char in = ((unsigned char *)SCRATCH_MEM)[p];
		printf("%02x",in);
	}
	for (;;);*/
}
#endif

void load_roms(int profile)
{
	struct SimpleDirEntry * entries = dir_entries(ROM_DIR);

	// TODO profile shifts settings, basic rom, os rom address

	//memory map of flash
	//0x200000: settings (32KB!)
	//0x208000: basic rom (32KB)
	//0x210000: os rom (64KB - 4x16KB)
	//0x220000: freezer rom (64KB)
	//0x230000: ?? (64KB)

#ifndef NO_FLASH
	if (profile == 4)
	{
#endif
		if (sd_present)
		{
			if (SimpleFile_OK == file_open_name_in_dir(entries, "ataribas.rom", files[5]))
			{
				loadosrom();
			}
			if (SimpleFile_OK == file_open_name_in_dir(entries, "atarixl.rom", files[5]))
			{
				loadosrom();
			}
			if (SimpleFile_OK == file_open_name_in_dir(entries, "freezer.rom", files[6]))
			{
				enum SimpleFileStatus ok;
				int len;
				ok = file_read(files[6], FREEZER_ROM_MEM, 0x10000, &len);
				if (ok == SimpleFile_OK && len == 0x10000) {
					LOG("freezer rom loaded\n");
					freezer_rom_present = 1;
				} else {
					LOG("loading freezer rom failed\n");
					freezer_rom_present = 0;
				}
			}
		}
#ifndef NO_FLASH
	}
	else
	{
		readFlash(romstart + 0x08000 + (profile<<13),0x2000,basic_addr());
		readFlash(romstart + 0x10000 + (profile<<14),0x4000,os_addr());
		readFlash(romstart + 0x20000,0x10000,FREEZER_ROM_MEM);
		freezer_rom_present = 1;
	}
#endif

	set_freezer_enable(freezer_rom_present);
}

void mainmenu()
{
	*atari_colbk = 0xb8;
#ifdef USB
	usb_init(&usb_porta,0);
#endif
#ifdef USB2
	usb_init(&usb_portb,1);
#endif
	*atari_colbk = 0x38;
	freezer_rom_present = 0;

	sd_present = !get_sd_detect();
	if (sd_present)
	{
		if (init_sd())
		{
			*atari_colbk = 0x18;
		}
		else
		{
			*atari_colbk = 0x0f;
			sd_present = 0;
			//for(;;);
			//printf("DIR init failed\n");
		}
	}

#ifndef NO_FLASH
	// Find rom settings location
	init_romstart();
#endif

	// default to flash
	load_roms(0);
	load_settings(0);

	// override if present on sd (do we really want this?)
	load_roms(4);
	load_settings(4);

	sel_profile = 1;

	init_drive_emulator();
	reboot(2);
	run_drive_emulator();
}

static char const * ramXL[] = 
{
	"64K",
	"128K",
	"320K(Compy)",
	"320K(Rambo)",
	"576K(Compy)",
	"576K(Rambo)",
	"1MB",
	"4MB"
};

static char const * ram800[] = 
{
	"8K",
	"16K",
	"32K",
	"48K",
	"52K"
	//TODO
};

static char const * key_types[] = 
{
	"ISO",
	"ANSI"
};

static char const * systemname[] = 
{
	"XL/XE",
	"400/800"
};

void menuPrintDrive(void * menuData, void * itemData)
{
	int i = (int)itemData;

	char buffer[20];
	describe_disk(i,&buffer[0]);
	printf("Drive %d:%s %s", i+1, file_name(files[i]), &buffer[0]);
}

void menuPrintCart(void * menuData, void * itemData)
{
	printf("Cart: %s", get_cart_select() ? file_name(files[4]) : "NONE");
}

void menuDrive(void * menuData, struct joystick_status * joy, int drive)
{
	if (!joy || joy->x_>0)
	{
		// Choose new disk
		filter = filter_disks;
		file_selector(files[drive]);
		set_drive_status(drive,files[drive]);
	}
	else if(joy->x_<0)
	{
		// Remove disk
		file_init(files[drive]);
		set_drive_status(drive,0);
	}
	else if (joy->fire_)
	{
		{
			// Swap files
			struct SimpleFile * temp = files[drive];
			files[drive] = files[0];
			files[0] = temp;
		}

		{
			// Swap disks
			struct SimpleFile * temp = get_drive_status(drive);
			set_drive_status(drive, get_drive_status(0));
			set_drive_status(0,temp);
		}
	}
}

int menuCart(void * menuData, struct joystick_status * joy)
{
	if (joy->x_>0) {
		fil_type = fil_type_car;
		filter = filter_specified;
		file_selector(files[4]);
		unsigned char mode = load_car(files[4]);
		set_cart_select(mode);
		if (mode) {
			return 1; 
		}
	}
	else if (joy->x_<0) {
		file_init(files[4]);
		set_cart_select(0);
	}
	return 0;
}

void menuDevicesHotkeys(void * menuData, unsigned char keyPressed)
{
	switch (keyPressed)
	{
	case '1':
	case '2':
	case '3':
	case '4':
		menuDrive(0,0,keyPressed-'1'); //TODO
		break;
	case 'C':
		menuCart(0,0); //TODO
		break;
#ifdef DEBUG_SUPPORT		
	case 'D':
		debug_menu();
		break;
#endif
	}
}

#ifdef USBSETTINGS
void menuRotateUSB(void * menuData, struct joystick_status * joy)
{
	rotate_usb_sticks();
}

void menuPrintUsb(void * menuData, void * itemData)
{
	usb_devices(debug_pos);
}
#endif

#ifdef DEBUG_SUPPORT
struct EditNumberState
{
	unsigned char col;
	unsigned char mask;
	unsigned char defaultShift;
};

struct DebugMenuData
{
	struct EditNumberState state[2];
};

void initDebugMenuData(struct DebugMenuData * menuData2)
{
	menuData2->state[0].col = 0;
	menuData2->state[0].mask = 0x3;
	menuData2->state[0].defaultShift = 12;
	menuData2->state[1].col = 0;
	menuData2->state[1].mask = 0x1;
	menuData2->state[1].defaultShift = 4;
}

void menuDisplayEditNumber(unsigned int num, struct EditNumberState * state)
{
	int shift = state->defaultShift;
	int i=0;
	int adj = debug_adjust;

	printf("0x");
	while(shift>=0)
	{
		int val = num;
		val = (val>> shift)&0xf;
		
		if (state->col == i)
			debug_adjust = adj^128;
		else
			debug_adjust = adj;
		printf("%1x",val);
		shift = shift-4;
		i=i+1;
	}

	debug_adjust = adj;
}

void menuPrintDebugAddress(void * menuData, void * itemData)
{
	printf("Addr:");
	struct DebugMenuData * menuData2 = (struct DebugMenuData *)(menuData);
	menuDisplayEditNumber(get_debug_addr(),&menuData2->state[0]);
}
void menuPrintDebugData(void * menuData, void * itemData)
{
	printf("Data:");
	struct DebugMenuData * menuData2 = (struct DebugMenuData *)(menuData);
	menuDisplayEditNumber(get_debug_data(),&menuData2->state[1]);
}
void menuPrintDebugMode(void * menuData, void * itemData)
{
	printf("Mode:");
	if (get_debug_read_mode())
		printf("Read ");
	if (get_debug_write_mode())
		printf("Write ");
	if (get_debug_data_match())
		printf("Match ");
}

bool hexDigitToNumber(unsigned char digit, int *val)
{
	if (digit>='0' && digit<='9')
	{
		*val = digit-'0';
		return true;
	}
	else if (digit>='A' && digit<='F')
	{
		*val = 10 + digit-'A';
		return true;
	}
	return false;
}

bool menuEditNumber(struct EditNumberState * state, struct joystick_status * joy, unsigned int * data)
{
	unsigned int val = 0;
	unsigned int mask = 0;
	int i;
	int shift = state->defaultShift;
	bool res = false;
	if (hexDigitToNumber(joy->keyPressed_,&val))
	{
		//key pressed, store the number in the current location
		for (i=0;i!=state->col;++i)
			shift = shift-4;

		mask = 0xf;
		val = val<<shift;
		mask = mask<<shift;

		*data = ((*data)&~mask) | val;

		state->col = state->col+1;
		res = true;
	}
	else if (joy->keyPressed_==-1)
	{
		// delete pressed, move back one place
		state->col = state->col-1;
	}

	state->col = state->col & state->mask; // sanitize col

	return res;
}

void menuDebugAddress(void * menuData, struct joystick_status * joy, int j)
{
	int i;
	struct DebugMenuData * menuData2 = (struct DebugMenuData *)(menuData);
	
	unsigned int addr = get_debug_addr();
	if (menuEditNumber(&menuData2->state[j],joy,&addr))
		set_debug_addr(addr);
}
void menuDebugData(void * menuData, struct joystick_status * joy, int j)
{
	int i;
	struct DebugMenuData * menuData2 = (struct DebugMenuData *)(menuData);

	unsigned int data = get_debug_data();

	if (menuEditNumber(&menuData2->state[j],joy,&data))
		set_debug_data(data);
}
void menuDebugHotkeys(void * menuData, unsigned char keyPressed)
{
	switch (keyPressed)
	{
	case 'R':
		set_debug_read_mode(!get_debug_read_mode());
		break;
	case 'W':
		set_debug_write_mode(!get_debug_write_mode());
		break;
	case 'M':
		set_debug_data_match(!get_debug_data_match());
		break;
	}
}

int debug_menu()
{
	struct MenuEntry entries[] = 
	{
		{&menuPrintDebugAddress,0,&menuDebugAddress,MENU_FLAG_KEYPRESS},
		{&menuPrintDebugData,1,&menuDebugData,MENU_FLAG_KEYPRESS},
		{&menuPrintDebugMode,0,0,MENU_FLAG_KEYPRESS},
		{0,0,0,0}, //blank line
		{0,"Exit",0,MENU_FLAG_EXIT},
		{0,0,0,MENU_FLAG_FINAL} //blank line
	};

	struct DebugMenuData menuData;
	initDebugMenuData(&menuData);
	return display_menu("Debug",&entries[0], &menuDebugHotkeys, &menuData);
}
#endif

int devices_menu()
{
	struct MenuEntry entries[] = 
	{
		{&menuPrintDrive,0,&menuDrive,MENU_FLAG_MOVE|MENU_FLAG_FIRE|MENU_FLAG_SD},
		{&menuPrintDrive,1,&menuDrive,MENU_FLAG_MOVE|MENU_FLAG_FIRE|MENU_FLAG_SD},
		{&menuPrintDrive,2,&menuDrive,MENU_FLAG_MOVE|MENU_FLAG_FIRE|MENU_FLAG_SD},
		{&menuPrintDrive,3,&menuDrive,MENU_FLAG_MOVE|MENU_FLAG_FIRE|MENU_FLAG_SD},
		{&menuPrintCart,0,&menuCart,MENU_FLAG_MOVE|MENU_FLAG_SD|MENU_FLAG_MAYEXIT},
#ifdef USBSETTINGS
		{0,"Rotate USB joysticks",&menuRotateUSB,MENU_FLAG_FIRE}, 
#endif
		{0,0,0,0}, //blank line
#ifdef DEBUG_SUPPORT
		{0,"Debug",&debug_menu,MENU_FLAG_FIRE},
		{0,0,0,0}, //blank line
#endif
		{0,"Exit",0,MENU_FLAG_EXIT},
		{0,0,0,0}, //blank line
#ifdef USBSETTINGS
		{&menuPrintUsb,0,0,MENU_FLAG_FINAL}
#else
		{0,0,0,MENU_FLAG_FINAL} //blank line
#endif
	};

	return display_menu("Devices",&entries[0], &menuDevicesHotkeys, 0);
}

char const * const * ram;
int max_ram_select;
void set_system()
{
	if (get_atari800mode() == 1)
	{
		ram = ram800;
		max_ram_select = MAX_RAM_SELECT_800;
	}
	else
	{
		ram = ramXL;
		max_ram_select = MAX_RAM_SELECT_XL;
	}
}

struct MenuData
{
	int video_mode;
	int tv;
	int scanlines;
	int csync;

#ifndef NO_FLASH
	int flashid1;
	int flashid2;
	int sectorSize;
#endif
};

void updateMenuDataVideoMode(struct MenuData * menuData2)
{
	menuData2->video_mode = get_video();
	menuData2->tv = get_tv();
	menuData2->scanlines = get_scanlines();
	menuData2->csync = get_csync();
}

void updateMenuDataFlashInfo(struct MenuData * menuData2)
{
#ifndef NO_FLASH
	readFlashId(&menuData2->flashid1,&menuData2->flashid2);
	menuData2->sectorSize = flashSectorSize();
#endif
}

void menuPrintProfile(void * menuData, void * itemData)
{
	printf("Profile:%d",sel_profile);
}


void menuProfileLoad(void * menuData)
{
	load_roms(sel_profile-1);
	load_settings(sel_profile-1);

	struct MenuData * menuData2 = (struct MenuData *)menuData;
	updateMenuDataVideoMode(menuData2);

	set_system();
}

void menuProfile(void * menuData, struct joystick_status * joy)
{
	sel_profile += joy->x_;
	if (sel_profile > 4)
		sel_profile = 4;
	if (sel_profile < 1)
		sel_profile = 1;

	if (joy->fire_)
	{
		menuProfileLoad(menuData);
	}
}

void menuPrintCpu(void * menuData, void * itemData)
{
	printf("CPU:%dx%s", get_turbo_6502(), get_turbo_6502_vblank_only() ? " VB only" : "");
}

void menuCpu(void * menuData, struct joystick_status * joy)
{
	int turbo = get_turbo_6502();
	if (joy->fire_) set_turbo_6502_vblank_only(!get_turbo_6502_vblank_only());
	if (joy->x_==1) turbo<<=1;
	if (joy->x_==-1) turbo>>=1;
	if (turbo>32) turbo = 32;
	if (turbo<1) turbo = 1;
	set_turbo_6502(turbo);
}

void menuPrintDriveTurbo(void * menuData, void * itemData)
{
	printf("Drive Turbo:%s", get_turbo_drive_str());
}

void menuDriveTurbo(void * menuData, struct joystick_status * joy)
{
	int turbo = get_turbo_drive();
	turbo+=joy->x_;
	if (turbo<0) turbo = 0;
	if (turbo>7) turbo = 7;
	set_turbo_drive(turbo);
}

void menuPrintSystem(void * menuData, void * itemData)
{
	printf("System:%s %s", ram[get_ram_select()], systemname[get_atari800mode()]);
}

void menuSystem(void * menuData, struct joystick_status * joy)
{
	int ram_select = get_ram_select();
	ram_select+=joy->x_;

	if (joy->fire_)
	{
		set_atari800mode(!get_atari800mode());
		set_system();
	}

	if (ram_select<0) ram_select = 0;
	if (ram_select>max_ram_select) ram_select = max_ram_select;
	set_ram_select(ram_select);
}

void menuPrintOS(void * menuData, void * itemData)
{
	printf("ROM:%s", file_name(files[5]));
}

void menuOS(void * menuData, struct joystick_status * joy)
{
	fil_type = fil_type_rom;
	filter = filter_specified;
	file_selector(files[5]);
	loadosrom();
}

void menuPrintKeyboard(void * menuData, void * itemData)
{
	printf("Keyboard:%s", key_types[get_key_type()]);
}

void menuKeyboard(void * menuData, struct joystick_status * joy)
{
	set_key_type(!get_key_type());
}

#ifndef NO_VIDEO_MODE
void menuPrintVideoMode(void * menuData, void * itemData)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	printf("Mode:%s", get_video_mode(menuData2->video_mode));
}

void menuVideoMode(void * menuData, struct joystick_status * joy)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	menuData2->video_mode = menuData2->video_mode + joy->x_;
	if (menuData2->video_mode > MAX_VIDEO_MODE)
		menuData2->video_mode = MAX_VIDEO_MODE;
	if (menuData2->video_mode < 0)
		menuData2->video_mode = 0;
}
#endif

void menuPrintTVStandard(void * menuData, void * itemData)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	printf("TV standard:%s", get_tv_standard(menuData2->tv));
}

void menuTVStandard(void * menuData, struct joystick_status * joy)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	menuData2->tv = !menuData2->tv;
}

void menuApplyVideo(void * menuData, struct joystick_status * joy)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	set_video(menuData2->video_mode);
	set_tv(menuData2->tv);
	set_scanlines(menuData2->scanlines);
	set_csync(menuData2->csync);
#ifdef PLL_SUPPORT
	set_pll(get_tv()==TV_PAL, get_video()>=VIDEO_HDMI && get_video()<VIDEO_COMPOSITE);
#endif
}

#ifndef NO_VIDEO_MODE
void menuPrintScanlines(void * menuData, void * itemData)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	printf("Scanlines:%d", menuData2->scanlines);
}

void menuScanlines(void * menuData, struct joystick_status * joy)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	menuData2->scanlines = !menuData2->scanlines;
}

void menuPrintCompositeSync(void * menuData, void * itemData)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	printf("Composite sync:%d", menuData2->csync);
}

void menuCompositeSync(void * menuData, struct joystick_status * joy)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	menuData2->csync = !menuData2->csync;
}
#endif

void menuSettingsHotKeys(void * menuData, unsigned char keyPressed)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;

#ifdef VIDEO_TV_ONLY
	int apply = 1;
	switch(keyPressed)
	{
	case 'P': // PAL
		menuData2->tv = TV_PAL;
		break;
	case 'N': // NTSC
		menuData2->tv = TV_NTSC;
		break;
	case '1':
	case '2':
	case '3':
	case '4':
		sel_profile = keyPressed - '0';

		menuProfileLoad(menuData);
		break;
	default:
		apply = 0;
	}

	if (apply==1)
	{
		menuApplyVideo(menuData,0);
	}
#endif

#ifndef NO_VIDEO_MODE
	int apply = 1;
	switch(keyPressed)
	{
	case 'P': // PAL
		menuData2->tv = TV_PAL;
		break;
	case 'N': // NTSC
		menuData2->tv = TV_NTSC;
		break;

	case 'R': // RGB
		menuData2->video_mode = VIDEO_RGB;
		break;
	case 'A': // scAndouble (S taken for svideo, C taken for composite)
		menuData2->video_mode = VIDEO_SCANDOUBLE;
		break;
	case 'D': // DVI
		menuData2->video_mode = VIDEO_DVI;
		break;
	case 'H': // HDMI
		menuData2->video_mode = VIDEO_HDMI;
		break;
	case 'V': // VGA
		menuData2->video_mode = VIDEO_VGA;
		break;
	case 'S': // Svideo
		menuData2->video_mode = VIDEO_SVIDEO;
		break;
	case 'C': // Composite
		menuData2->video_mode = VIDEO_COMPOSITE;
		break;
	case 'Z': // Composite sync toggle
		menuData2->csync  = !menuData2->csync;
		break;
	case 'X': // Scanlines toggle
		menuData2->scanlines  = !menuData2->scanlines;
		break;
	case '1':
	case '2':
	case '3':
	case '4':
		sel_profile = keyPressed - '0';

		menuProfileLoad(menuData);
		break;
	default:
		apply = 0;
	}

	if (menuData2->video_mode > MAX_VIDEO_MODE)
		menuData2->video_mode = MAX_VIDEO_MODE;
	if (apply==1)
	{
		menuApplyVideo(menuData,0);
	}
#endif
}

#ifndef NO_FLASH
void menuSaveFlash(void * menuData, struct joystick_status * joy)
{
	save_settings(sel_profile-1);
}
#endif

void menuSaveSD(void * menuData, struct joystick_status * joy)
{
	save_settings(4);
}

#ifdef RPD_SUPPORT
void menuProgramRBD(void * menuData, struct joystick_status * joy)
{
	fil_type = fil_type_rpd;
	filter = filter_specified;
	file_selector(files[4]);
	flash_rpd(files[4]);
}
#endif

void menuStatus1(void * menuData, struct joystick_status * joy)
{
#ifndef NO_VID2I2C
	printf("Board:%d %s %s%s%s",*zpu_board,"Date:YYYYMMDD Core:XX",isHDMIConnected() ? "HDMI " : "",isVGAConnected() ? "VGA ":"",sd_present ? "SD ":"");
#else
	printf("Board:%d %s %s",*zpu_board,"Date:YYYYMMDD Core:XX",sd_present ? "SD ":"");
#endif
}

#ifndef NO_FLASH
void menuStatus2(void * menuData, struct joystick_status * joy)
{
	struct MenuData * menuData2 = (struct MenuData *)menuData;
	printf("SPI:%08x/%08x/%d",menuData2->flashid1,menuData2->flashid2,menuData2->sectorSize);
}
#endif

int settings_menu()
{
	struct MenuEntry entries[] = 
	{
		{&menuPrintProfile,0,&menuProfile,MENU_FLAG_MOVE|MENU_FLAG_FIRE},
		{0,0,0,0}, //blank line
		{&menuPrintCpu,0,&menuCpu,MENU_FLAG_MOVE|MENU_FLAG_FIRE},
		{&menuPrintDriveTurbo,0,&menuDriveTurbo,MENU_FLAG_MOVE},
		{&menuPrintSystem,0,&menuSystem,MENU_FLAG_MOVE|MENU_FLAG_FIRE},
		{&menuPrintOS,0,&menuOS,MENU_FLAG_FIRE},
		{&menuPrintKeyboard,0,&menuKeyboard,MENU_FLAG_FIRE},
		{0,0,0,0}, //blank line
#ifndef NO_VIDEO_MODE
		{&menuPrintVideoMode,0,&menuVideoMode,MENU_FLAG_MOVE},
		{&menuPrintTVStandard,0,&menuTVStandard,MENU_FLAG_FIRE},
		{&menuPrintScanlines,0,&menuScanlines,MENU_FLAG_FIRE},
		{&menuPrintCompositeSync,0,&menuCompositeSync,MENU_FLAG_FIRE},
		{0,"Apply video",&menuApplyVideo,MENU_FLAG_FIRE},
		{0,0,0,0}, //blank line
#endif
#ifdef VIDEO_TV_ONLY
		{&menuPrintTVStandard,0,&menuTVStandard,MENU_FLAG_FIRE},
		{0,"Apply video",&menuApplyVideo,MENU_FLAG_FIRE},
		{0,0,0,0}, //blank line
#endif
#ifndef NO_FLASH
		{0,"Save Flash",&menuSaveFlash,MENU_FLAG_FIRE},
#endif
		{0,"Save SD",&menuSaveSD,MENU_FLAG_FIRE|MENU_FLAG_SD}, 
#ifdef RPD_SUPPORT
		{0,"Program RBD",&menuProgramRBD,MENU_FLAG_FIRE|MENU_FLAG_SD}, 
#endif
		{0,0,0,0}, //blank line
#ifndef NO_FLASH
		{0,"Exit",0,MENU_FLAG_EXIT},
#else
		{0,"Exit",0,MENU_FLAG_FINAL|MENU_FLAG_EXIT},
#endif
		{0,0,0,0}, //blank line
#ifndef NO_FLASH
		{&menuStatus1,0,0,0},
		{&menuStatus2,0,0,MENU_FLAG_FINAL},
#else
		{&menuStatus1,0,0,MENU_FLAG_FINAL},
#endif
	};

	set_system(); // sets up ram variable!
	struct MenuData menuData;
	updateMenuDataVideoMode(&menuData);
	updateMenuDataFlashInfo(&menuData);

	display_menu("Settings",&entries[0], &menuSettingsHotKeys, &menuData);
	return 0;

/*		case 9:
		case 10:
			{
				if (joy.fire_)
				{
					fil_type = fil_type_mem;
					filter = filter_specified;
					file_selector(files[6]);
					if (row == 9)
					{
						freeze_load(files[6]);
					}
					else if (row == 10)
					{
						freeze_save(files[6]);
					}
				}
			}
			break;*/
}

void update_keys()
{
	#ifdef LINUX_BUILD
		check_keys();
	#endif
	#ifdef USB
		usb_poll(&usb_porta);
	#endif
	#ifdef USB2
		usb_poll(&usb_portb);
	#endif
}

void actions()
{
	int sd_really_present;

	update_keys();

	sd_really_present = !get_sd_detect();
	if (sd_present==0 && sd_really_present==1)
	{
		if (init_sd())
		{
			init_drive_emulator();
		}
		else
		{
			sd_really_present = 0;
		}
	}
	if (sd_present==1 && sd_really_present==0)
	{
		int i;
		init_drive_emulator();
		for (i=0; i!=NUM_FILES; ++i)
		{
			file_init(files[i]);
		}
	}
	sd_present = sd_really_present;


	// Show some activity!
	//*atari_colbk = *atari_random;
	
	// Hot keys
	if (get_hotkey_softboot())
	{
		reboot(0);	
	}
	else if (get_hotkey_coldboot())
	{
		reboot(1);	

		do
		{
			update_keys();
		}
		while (get_hotkey_coldboot());
	}
	else if (get_hotkey_settings())
	{
		set_pause_6502(1);
		set_freezer_enable(0);
		freeze();
		debug_pos = 0;	
		int do_reboot = settings_menu();
		debug_pos = -1;
		restore();
		if (do_reboot)
			reboot(1);
		else {
			set_freezer_enable(freezer_rom_present);
			set_pause_6502(0);
		}
	}
	else if (get_hotkey_fileselect())
	{
/*#ifdef USB
		set_pause_6502(1);
		set_freezer_enable(0);
		freeze();

		debug_pos = 0;	
		printf("Hello USB");
		debug_pos = 80;
		usb_init();
		while (1)
		{
			usb_poll();
			if (debug_pos>1000)
			{
				debug_pos = 80;
			}
		}

		debug_pos = -1;
		restore();
		set_freezer_enable(freezer_rom_present);
		set_pause_6502(0);
#else*/
		set_pause_6502(1);
		set_freezer_enable(0);
		freeze();
		debug_pos = 0;	
		int do_reboot = devices_menu();
		debug_pos = -1;
		restore();
		if (do_reboot)
			reboot(1);
		else {
			set_freezer_enable(freezer_rom_present);
			set_pause_6502(0);
		}
	}
}




