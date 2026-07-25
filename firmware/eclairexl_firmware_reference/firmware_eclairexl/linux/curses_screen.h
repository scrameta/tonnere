#ifndef CURSES_SCREEN_H
#define CURSES_SCREEN_H
#include "joystick.h"

int init_curses_screen(void);

void deinit_curses_screen(void);

void display_out_regs(void);

void display_char(int x, int y, char c, int inverse);

void clearscreen(void);

void check_keys(void);

void print_log(const char* format, ...) __attribute__ ((format (printf, 1, 2))) ;

#endif
