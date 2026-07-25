#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <curses.h>
#include "curses_screen.h"
#include "linux_helper.h"
#include "regs.h"

static WINDOW* atari_window;
static WINDOW* status_window;
static WINDOW* log_window;

static attr_t atari_color;
static attr_t atari_inverse_color;
static attr_t status_color;
static attr_t log_color;

int init_curses_screen(void)
{
	int height;
	int width;

	initscr();

	getmaxyx(stdscr, height, width);

	if (height < 24 || width < 80) {
		fprintf(stderr, "screen is too small (must be 80x24)\n");
		return 1;
	}

	nonl();
	cbreak();
	noecho();

	atari_window = newwin(24, 40, 0, 0);
	status_window = newwin(4, width - 40, 0, 40);
	log_window = newwin(height - 4, width - 40, 4, 40);

	atari_color = 0;
	atari_inverse_color = A_REVERSE;
	if (has_colors()) {
		start_color();
		init_pair(1, COLOR_YELLOW, COLOR_BLUE);
		init_pair(2, COLOR_YELLOW, COLOR_BLACK);
		status_color = COLOR_PAIR(1);
		log_color = COLOR_PAIR(2);
	} else {
		status_color = A_REVERSE;
		log_color = 0;
	}
	wbkgdset(atari_window, atari_color | ' ');
	wbkgdset(log_window, log_color | ' ');
	wbkgdset(status_window, status_color | ' ');
	wbkgdset(log_window, log_color | ' ');

	keypad(stdscr, TRUE);
	scrollok(log_window, TRUE);

	werase(atari_window);
	werase(status_window);
	werase(log_window);

	curs_set(0);

	refresh();
	return 0;
}

void deinit_curses_screen(void)
{
	endwin();
}

#define MAXSTR 256
static char tmpstr[MAXSTR];

void display_out_regs(void)
{
	uint32_t out1 = *zpu_out1;

	werase(status_window);
	snprintf(tmpstr, MAXSTR, "out1: %08x", out1);
	mvwaddstr(status_window, 0, 1, tmpstr);

	snprintf(tmpstr, MAXSTR, "- - tur %02x ram %02x rom %02x car %02x frz %1d",
		(out1 >> 2) & 0x3f,
		(out1 >> 8) & 0x07,
		(out1 >> 11) & 0x3f,
		(out1 >> 17) & 0x3f,
		(out1 >> 25) & 1);
	if (out1 & 1) {
		tmpstr[0] = 'P';
	}
	if (out1 & 2) {
		tmpstr[2] = 'R';
	}
	mvwaddstr(status_window, 1, 1, tmpstr);

	wrefresh(status_window);
}

void display_char(int x, int y, char c, int inverse)
{
	if (inverse) {
		wattrset(atari_window, atari_inverse_color);
	}
	mvwaddch(atari_window, y, x, c);
	wattrset(atari_window, atari_color);

	wrefresh(atari_window);
}

void clearscreen(void)
{
	wattrset(atari_window, atari_color);
	werase(atari_window);
	wrefresh(atari_window);
}

void check_keys(void)
{
	int ch;
	*zpu_in1 = 0;

	while (1) {
		ch = wgetch(stdscr);

		switch (ch) {
		case KEY_F(12):
			*zpu_in1 = 1<<11; return;
		case KEY_F(11):
			*zpu_in1 = 1<<10; return;
		default:
			*zpu_in1 = 0; return;
		}
	}
}

void joystick_poll(struct joystick_status * status)
{
	// we are blocking instead of polling
	int ch = wgetch(stdscr);

	status->x_ = 0;
	status->y_ = 0;
	status->fire_ = 0;
	status->escape_ = 0;


	switch (ch) {
	case KEY_UP: status->y_ = -1; break;
	case KEY_DOWN: status->y_ = 1; break;
	case KEY_LEFT: status->x_ = -1; break;
	case KEY_RIGHT: status->x_ = 1; break;
	case KEY_ENTER:
	case 13:
		status->fire_ = 1; break;
	case 27: // ESCAPE:
		status->escape_ = 1; break;
	default: break;
	}
}

void joystick_wait(struct joystick_status * status, enum JoyWait waitFor)
{
	if (waitFor == WAIT_QUIET)
	{
		status->x_ = 0;
		status->y_ = 0;
		status->fire_ = 0;
		return;
	}
	while (1)
	{
		joystick_poll(status);
		switch (waitFor)
		{
		case WAIT_QUIET:
			break;
		case WAIT_FIRE:
			if (status->fire_ == 1) return;
			break;
		case WAIT_EITHER:
			if (status->fire_ == 1) return;
			// fall through
		case WAIT_MOVE:
			if (status->x_ !=0 || status->y_ != 0) return;
			break;
		}
	}
}

void print_log(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);
	vw_printw(log_window, format, arg);
	va_end(arg);
	wrefresh(log_window);
}

