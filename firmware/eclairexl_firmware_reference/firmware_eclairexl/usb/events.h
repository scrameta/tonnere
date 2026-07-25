//#include "printf.h"
#include "regs.h"

#include "keycodes.h"

extern int debug_pos;

uint8_t kbbuf[6];
uint32_t jmaps[4];

int8_t analogx[4];
int8_t analogy[4];

void event_keyboard(uint8_t mod, uint8_t buf[])
{
	//printf("Event keyboard:%d\n", mod);
	/*int changed = 0;

	if (lastmod!=i)
	{
		lastmod = i;
		changed = 1;
	}
	int j;
        for (j=0;j!=8;++j)
	{
		//printf("Event keyboard:%d = %x\n", j, buf[j]);
		if (buf[j] != kbbuf[j])
		{
			changed = 1;
			kbbuf[j] = buf[j];
		}
	}*/

  /* usb modifer bits: 
        0     1     2    3    4     5     6    7
      LCTRL LSHIFT LALT LGUI RCTRL RSHIFT RALT RGUI
  */

	// Convert changes into a serial of release/press notifications
	int i=0;
	for(i=0;i!=8;++i)
	{
		int bit = 1<<i;
		uint32_t pressed = !!(bit&mod);
		uint32_t ps2_key = ps2_modifier[i];
		*zpu_out4 = (pressed<<16)|ps2_key;
	}
	
	// unpress old keys that are no longer pressed
	for (i=0;i!=6;++i)
	{
		if (kbbuf[i]==MISS) continue;

		uint32_t oldkey = usb2ps2[kbbuf[i]];
		if (oldkey==MISS) continue;

		int j=0;
		int found = 0;
		for (j=0;j!=6;++j)
		{
			uint32_t newkey = usb2ps2[buf[i]];
			if (oldkey==newkey) {found=1;break;}
		}

		if (!found)
		{
			*zpu_out4 = oldkey; // unpress
		}
	}

	// press/hold new keys
	for (i=0;i!=6;++i)
	{
		if (buf[i]==MISS) continue;

		uint32_t newkey = usb2ps2[buf[i]];

		if (newkey==MISS) continue;

		// press new one
		*zpu_out4 = (1<<16)|newkey;
	}

	for (i=0;i!=6;++i)
	{
		kbbuf[i] = buf[i]; // store
	}

/*	if (changed)
	{
		debug_pos = 120;
		printf("KB:");
		printf("%02x ",kbi);
		for (j=0;j!=8;++j)
		{
			printf("%04x ",usb2ps2[kbbuf[j]]);
		}

		// lctrl:1, lshift:2, lalt:4, lwin:8, rctrl:10, rshift:20, altgr:40
		uint32_t mod = kbi;
		uint32_t key1 = usb2ps2[kbbuf[0]];
		//uint32_t key2 = usb2ps2[kbbuf[1]];
		//uint32_t key3 = usb2ps2[kbbuf[2]];

		uint32_t res = (key3<<24) | (key2<<16) || (key1<<8) || mod;
		*zpu_out4 = res;
	}*/
}
void event_mouse(uint8_t a, uint8_t b, uint8_t c)
{
	//printf("Event mouse:%d %d %d\n",a,b,c);
}
void event_digital_joystick(uint8_t idx, uint32_t jmap)
{
	//printf("Event joystick:%d %x\n", idx,jmap);
	/*if (jmaps[idx] != jmap)
	{
		jmaps[idx] = jmap;
		debug_pos = 200 + idx*40;
		printf("JOY:%d:%08x ",idx,jmap);
	}*/
	idx = idx&1;
	if (idx == 0)
	{
		*zpu_out2 = jmap;
	}
	if (idx == 1)
	{
		*zpu_out3 = jmap;
	}
}
void event_analog_joystick(uint8_t idx, int8_t x, int8_t y)
{
	//printf("Event analog joystick:%d %d %d\n", idx,x,y);
	idx = idx&1;
	if (analogx[idx]!=x || analogy[idx]!=y)
	{
		analogx[idx] = x;
		analogy[idx] = y;
		//debug_pos = 360 + idx*40;
		//printf("AJOY:%d:%d:%d ", idx,x,y);
		
		uint32_t x0 = (uint8_t)analogx[0];
		uint32_t y0 = (uint8_t)analogy[0];
		uint32_t x1 = (uint8_t)analogx[1];
		uint32_t y1 = (uint8_t)analogy[1];

		uint32_t comb = (y1<<24)|(x1<<16)|(y0<<8) |x0;
		*zpu_out5 = comb;
	}
}

#define JOY_RIGHT       0x01
#define JOY_LEFT        0x02
#define JOY_DOWN        0x04
#define JOY_UP          0x08
#define JOY_BTN1        0x10
#define JOY_BTN2        0x20
#define JOY_BTN3        0x40
#define JOY_BTN4        0x80
#define JOY_MOVE        (JOY_RIGHT|JOY_LEFT|JOY_UP|JOY_DOWN)


