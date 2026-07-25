#include "joystick.h"

#include <stdio.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

extern char native_porta;
extern char native_trig;

void read_keys();

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

int main(void)
{
	term_init();

	fprintf(stderr,"Polling\n");
	int i;
	for (i=0;i!=200;++i)
	{
		struct joystick_status joy;
		read_keys();
		joystick_poll(&joy);

		fprintf(stderr, "x:%d y:%d fire:%d\n", joy.x_, joy.y_, joy.fire_);

		usleep(100000);
	}


//void joystick_poll(struct joystick_status * status);
//void joystick_wait(struct joystick_status * status, enum JoyWait waitFor);

	term_close();

	return 0;
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

		if (buffer[0] == 0x41) fprintf(stderr, "UP\n");
		if (buffer[0] == 0x42) fprintf(stderr, "DOWN\n");
		if (buffer[0] == 0x44) fprintf(stderr, "LEFT\n");
		if (buffer[0] == 0x43) fprintf(stderr, "RIGHT\n");
		if (buffer[0] == 0x20) fprintf(stderr, "FIRE\n");

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


