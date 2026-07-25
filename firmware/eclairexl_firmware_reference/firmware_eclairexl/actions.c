int bit_set(int var, int bit)
{
	return (((1<<bit)&var)!=0);
}

void actions()
{
	unsigned int i = 0;
	//unsigned volatile char * store = 0xf00000; // SDRAM - fails!!
	//unsigned volatile char * store  = 0xf80000; // SRAM...
	//unsigned volatile char * store2 = 0xfc0000; // custom chips...

	// cold start (need to clear a few key locations to make OS cold start)
	// file selector (where applicable)
	// options (where applicable)

	int keys = *zpu_in;

	if (bit_set(keys,0))
	{
		coldstart();
	}
	else if (bit_set(keys,1))
	{
		set_pause_6502(1);
		freeze();
		menu_options();
		restore();
		set_pause_6502(0);
	}
	else if (bit_set(keys,2))
	{
		set_pause_6502(1);
		freeze();
		menu_fileselector();
		coldstart();
	}
}

void menu_options()
{
	// title
	// memory
	// rom
	// turbo
	// disks
	// exit/reboot

	// simple state machine for menu, so I set up a small data structure then it just runs from that...
}

void menu_fileselector()
{
	// title
	// loads of stuff, filtered by type
	// directories can be selected
	// initial directory set, after that starts where it was left
}

