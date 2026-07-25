#include "uart.h"

#include <stdio.h>

char native_command;

char receive_buffer[1024*1024];
int receive_buffer_pos=0;
int receive_buffer_last=0;

void USART_Init( u08 value )
{
	fprintf(stderr, "USART_Init:%d\n",value);
}

void USART_Transmit_Byte( unsigned char data )
{
	fprintf(stderr, "USART_Transmit:%02x\n", data);
}

unsigned char USART_Receive_Byte( void )
{
	unsigned char res = receive_buffer[receive_buffer_pos++];
	fprintf(stderr, "USART_Receive:%02x\n",res);
	return res;
}

int USART_Data_Ready()
{
	return 0;
}

void USART_Transmit_Mode()
{
	fprintf(stderr,"USART_Transmit mode\n");
}

void USART_Receive_Mode()
{
	fprintf(stderr,"USART_Receive mode\n");
}

int USART_Framing_Error()
{
	return 0;
}

void USART_Wait_Transmit_Complete()
{
	return;
}

int USART_Command_Line()
{
	return native_command;
}

