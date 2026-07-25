

unsigned char toatarichar(int val)
{
	if (val>='A' && val<='Z')
	{
		val+=-'A'+33;
	}
	else if (val>='a' && val<='z')
	{
		val+=-'a'+33+64;
	}
	else if (val>='0' && val<='9')
	{
		val+=-'0'+16;	
	}
	else
	{
		val = 0;
	}
	return val;
}

int xpos = 0;
int ypos = 0;

void nextline()
{
	xpos=0;
	ypos+=1;
	if (ypos==24)
		ypos = 0;
}
void plot(unsigned char a, int x, int y)
{
	printf("%d %d %d\n",a,x,y);
}

void debug(char const * str)
{
//        char buffer[256];
//        buffer[0] = 'W';
//        buffer[1] = 'T';
//        buffer[2] = 'F';
//        buffer[3] = 0;
//        str = buffer;

        while (1)
        {
                int val = *str++;
                if (val==0) break;

                plot(toatarichar(val),xpos,ypos);
                ++xpos;
                if (xpos==40)
                {
                        nextline();
                }
        }
        nextline();
        //Delay100usX(10000);
}


int main(void)
{
	printf("%d %d\n", 'H', toatarichar('H'));
	debug("Goodbye sweet world!");
	return 0;
}

