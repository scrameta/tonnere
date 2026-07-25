#include "joystick.h"

#include "regs.h"
//#include "io.h"

//#include <stdio.h>

#ifdef USB
#include "usb.h"
#endif

#ifdef USB
extern struct usb_host usb_porta;
#endif
#ifdef USB2
extern struct usb_host usb_portb;
#endif

/* TODO: probably simpler to output ascii from the vhdl */
static unsigned char bitascii[] = 
{
	63,//'A',
	21,//'B',
	18,//'C',
	58,//'D',
	42,//'E',
	56,//'F',
	61,//'G',
	57,//'H',
	13,//'I',
	1,//'J',
	5,//'K',
	0,//'L',
	37,//'M',
	35,//'N',
	8,//'O',
	10,//'P',
	47,//'Q',
	40,//'R',
	62,//'S',
	45,//'T',
	11,//'U',
	16,//'V',
	46,//'W',
	22,//'X',
	43,//'Y',
	23,//'Z'
};

static unsigned char bitasciinum[] = 
{
	50,//'0',
	31,//'1',
	30,//'2',
	26,//'3',
	24,//'4',
	29,//'5',
	27,//'6',
	51,//'7',
	53,//'8',
	48,//'9',
};

int bit_set(unsigned int low, unsigned int high, int bit)
{
	if (bit<32)
	{
		return (low&(1<<bit))!=0;
	}
	else if (bit<64)
	{
		return (high&(1<<bit))!=0;
	}
	else
	{
		return 0;
	}
}

int decodeKey()
{
	unsigned int lowAtariKeys = *zpu_in3;
	unsigned int highAtariKeys = *zpu_in4;

	int i;
	for (i='A';i<='Z';++i)
	{
		if (bit_set(lowAtariKeys,highAtariKeys,bitascii[i-'A']))
			return i;
	}
	for (i='0';i<='9';++i)
	{
		if (bit_set(lowAtariKeys,highAtariKeys,bitasciinum[i-'0']))
			return i;
	}
	if (bit_set(lowAtariKeys,highAtariKeys,52)) //backspace
	{
		return -1;
	}
	return 0;
}

void joystick_poll(struct joystick_status * status)
{
	status->x_ = 0;
	status->y_ = 0;
	status->fire_ = 0;
	status->escape_ = 0;
	status->keyPressed_ = decodeKey();

#ifdef USB
	usb_poll(&usb_porta);
#endif
#ifdef USB2
	usb_poll(&usb_portb);
#endif

	unsigned char porta = *atari_porta;
	porta = (porta>>4) & (porta);

	int controls = get_controls();

	status->y_ = !(porta&0x2) -((unsigned int)!(porta&0x1));
	status->x_ = !(porta&0x8) -((unsigned int)!(porta&0x4));
	status->fire_ = !(1&*atari_trig0&*atari_trig1);

	if (controls!=0)
	{
		status->y_ = !!(controls&0x2) -((unsigned int)!!(controls&0x1));
		status->x_ = !!(controls&0x8) -((unsigned int)!!(controls&0x4));
		status->fire_ = !!(controls&0x10);
		status->escape_ = !!(controls&0x20);
	}

	//if (porta != 0xff)
	//printf("%02x %x %x %x\n",porta,status->x_,status->y_,status->fire_);
/*
	if (0==(porta&0x2)) // down
	{
		status->y_ =1;
	}
	else if (0==(porta&0x1)) // up
	{
		status->y_ =-1;
	}
	if (0==(porta&0x8)) // right
	{
		status->x_ = 1;
	}
	else if (0==(porta&0x4)) // left
	{
		status->x_ = -1;
	}
	if (0==(1&*atari_trig0)) // fire
	{
		status->fire_ = 1;
	}
*/
}

void joystick_wait(struct joystick_status * status, enum JoyWait waitFor)
{
	while (1)
	{
		joystick_poll(status);
		switch (waitFor)
		{
		case WAIT_QUIET:
			if (status->x_ == 0 && status->y_ == 0 && status->fire_ == 0 && status->keyPressed_ == 0) return;
			break;
		case WAIT_FIRE:
			if (status->fire_ == 1 || status->escape_==1) return;
			break;
		case WAIT_EITHER:
			if (status->fire_ == 1) return;
			// fall through
		case WAIT_MOVE:
			if (status->x_ !=0 || status->y_ != 0 || status->escape_==1 || status->keyPressed_ !=0) return;
			break;
		}
	}
}

