#include "joystick.h"
#include <poll.h>
#include <termios.h>
#include <unistd.h>

extern char native_porta;
extern char native_trig;

struct termios oldt, newt;
void term_init()
{
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt);
}

void term_close()
{
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

void read_keys()
{
	struct pollfd fds[1];
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	native_porta = 0xff;
	native_trig = 0xff;
	while (poll(&fds[0], 1, 0))
	{
		char buffer[0];
		read(0,&buffer[0],1);

	/*	if (buffer[0] == 0x41) fprintf(stderr, "UP\n");
		if (buffer[0] == 0x42) fprintf(stderr, "DOWN\n");
		if (buffer[0] == 0x44) fprintf(stderr, "LEFT\n");
		if (buffer[0] == 0x43) fprintf(stderr, "RIGHT\n");
		if (buffer[0] == 0x20) fprintf(stderr, "FIRE\n");*/

		// RLDU
		if (buffer[0] == 0x41) native_porta=0xff&~(1<<0);
		if (buffer[0] == 0x42) native_porta=0xff&~(1<<1);
		if (buffer[0] == 0x44) native_porta=0xff&~(1<<2);
		if (buffer[0] == 0x43) native_porta=0xff&~(1<<3);
		if (buffer[0] == 0x20) native_trig=0xff&~1;

		fds[0].fd = STDIN_FILENO;
		fds[0].events = POLLIN;
	}
}

#include "regs.h"

void joystick_poll(struct joystick_status * status)
{
	static int first = 1;
	if (first)
	{
		first = 0;
		term_init();
	}

	read_keys();

	status->x_ = 0;
	status->y_ = 0;
	status->fire_ = 0;
	status->escape_ = 0;

	unsigned char porta = *atari_porta;
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
}

void joystick_wait(struct joystick_status * status, enum JoyWait waitFor)
{
	while (1)
	{
		joystick_poll(status);
		switch (waitFor)
		{
		case WAIT_QUIET:
			if (status->x_ == 0 && status->y_ == 0 && status->fire_ == 0) return;
			break;
		case WAIT_FIRE:
			if (status->fire_ == 1) return;
			break;
		case WAIT_EITHER:
			if (status->fire_ == 1) return;
			// fall through
		case WAIT_MOVE:
			if (status->x_ != 0 || status->y_ != 0) return;
			break;
		}
	}
}


