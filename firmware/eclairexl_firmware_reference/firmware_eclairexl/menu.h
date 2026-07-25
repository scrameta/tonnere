#pragma once
/* Simple lightweight menu function */

/* menu entry:
 * i) display function or fixed text
 * ii) location based actions: left/right/fire -> function
 *  -> result: redraw, exit, nothing
 * iii) actions that are not impacted by location: typing
 * iv) no content
 */

#include "integer.h"
#include "joystick.h"

typedef void (*menuDisplay) (void * menuData, void * userData); /*Call printf...*/
typedef BOOL (*menuAction) (void * menuData, struct joystick_status *, void * userData);  /* return true to exit */

typedef void (*menuPress) (void * menuData, unsigned char key_ascii);

struct MenuEntry
{
	menuDisplay displayFunction;
	union
	{
		char * txt;
		void * userData;
		int val;
	};
	menuAction actionFunction;
	unsigned char flags;

	#define MENU_FLAG_EXIT 1
	#define MENU_FLAG_LEFT 2
	#define MENU_FLAG_RIGHT 4
	#define MENU_FLAG_MOVE (MENU_FLAG_LEFT|MENU_FLAG_RIGHT)
	#define MENU_FLAG_FIRE 8 
	#define MENU_FLAG_FINAL 16
	#define MENU_FLAG_MAYEXIT 32
	#define MENU_FLAG_SD 64
	#define MENU_FLAG_KEYPRESS 128
};

int display_menu(const char * title, struct MenuEntry * entries, menuPress hotkeys, void * menuData);

/*
 * Example menu
void menuPressTest(unsigned char key)
{
	printf("Key pressed:%c\n",key);
	wait_us(2000000);
}

BOOL menuFire(struct joystick_status * joy)
{
	printf("Fire:%d",joy->fire_);
	wait_us(2000000);
	return 0;
}

BOOL menuMove(struct joystick_status * joy)
{
	printf("Move:%d",joy->x_);
	wait_us(2000000);
	return 0;
}

struct MenuEntry entries[] = 
{
	{0,"Subtitle",0,0},
	{0,0,0,0}, //blank line
	{0,"Fire",&menuFire,MENU_FLAG_FIRE},
	{0,"Move",&menuMove,MENU_FLAG_MOVE},
	{0,"Test",0,MENU_FLAG_FINAL}
};

display_menu("Settings",&entries[0], &menuPressTest);

*/

