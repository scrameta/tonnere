#include "led.h"

#include "regs.h"

void set_green_led(int i)
{
	*zpu_ledg = i;
}
void set_red_led(int i)
{
	*zpu_ledr = i;
}
void set_drive_number(int i)
{
	*zpu_hex = i;
}
void init_display()
{
	*zpu_hex = 0;
}
