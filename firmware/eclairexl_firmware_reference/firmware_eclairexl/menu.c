#include "menu.h"

#include "joystick.h"
#include "printf.h"
#include "pause.h"

void clearscreen(); // TODO: header
void actions();
extern int debug_pos;
extern int debug_adjust;
extern unsigned char sd_present;

void memcp8(char const volatile * from, char volatile * to, int offset, int len);
void memset8(void * address, int value, int length);

#ifndef LINUX_BUILD
void *memcpy(void *dest, const void *src, int n)
{
	memcp8(src,dest,0,n);
	return dest;
}

void *memset(void * s, int constant, int size)
{
	memset8(s,constant,size);
	return s;
}
#endif

int display_menu(char const * title, struct MenuEntry * entries, menuPress hotkeys, void * menuData)
{
	struct joystick_status joy;
	joy.x_ = joy.y_ = joy.fire_ = joy.escape_ = 0;

	int row = 0;

	int res = 0;

	int done = 0;
	for (;!done;)
	{
		// Render
		clearscreen();
		debug_pos = 0;
		debug_adjust = 0;

		printf((char *)title);

		struct MenuEntry * entry = entries;
		debug_pos = 80;
		int entryRow = 0;
		int debug_pos_next = 80;
		BOOL has_action = 0;
		for (;;)
		{
			has_action = entry->actionFunction || entry->flags&MENU_FLAG_EXIT;
			debug_adjust = (has_action && row==entryRow) ? 128 : 0;
			debug_pos_next = debug_pos+40;
			if (entry->displayFunction)
				entry->displayFunction(menuData, entry->userData);
			else if (entry->txt)
				printf(entry->txt);
			if (entry->flags&MENU_FLAG_SD && !sd_present)
				printf("(No SD)");
			debug_pos = debug_pos_next;

			if (has_action)
				entryRow = entryRow+1;

			if (entry->flags&MENU_FLAG_FINAL)
				break;

			entry = entry + 1;
		}

		// Slow it down a bit
		wait_us(100000);

		// move
		joystick_wait(&joy,WAIT_QUIET);
		joystick_wait(&joy,WAIT_EITHER);
		if (joy.escape_) break;
		if (joy.keyPressed_ > 0 && hotkeys) 
		{
			hotkeys(menuData, joy.keyPressed_);
		}

		row+=joy.y_;
		if (row<0) row = 0;
		if (row>=entryRow) row = entryRow-1;

		actions();

		entry = entries;
		entryRow = 0;
		for (;;)
		{
			has_action = entry->actionFunction || entry->flags&MENU_FLAG_EXIT;
			if (has_action)
			{
				if (entryRow == row)
				{
					if (joy.fire_ && entry->flags&MENU_FLAG_EXIT)
					{
						done = 1;
					}

					if (entry->actionFunction)
					{
						if (entry->flags&MENU_FLAG_SD && !sd_present)
						{}
						else if ((entry->flags&MENU_FLAG_LEFT && joy.x_<0) || (entry->flags&MENU_FLAG_RIGHT && joy.x_>0) || (entry->flags&MENU_FLAG_FIRE && joy.fire_) || (entry->flags&MENU_FLAG_KEYPRESS && joy.keyPressed_!=0))
							if (entry->actionFunction(menuData, &joy, entry->userData))
								if (entry->flags&MENU_FLAG_MAYEXIT)
								{
									res = 1;
									done = 1;
								}
					}

					break;
				}
				entryRow = entryRow+1;
			}

			if (entry->flags&MENU_FLAG_FINAL)
				break;

			entry = entry + 1;
		}
	}
	return res;
}

