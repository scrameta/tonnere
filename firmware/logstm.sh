#!/bin/bash

if [[ "$1" == "" ]] then
	USB_PORT=/dev/ttyUSB0
else
	USB_PORT=$1
fi

if [[ ! -e $USB_PORT ]] then
	echo "No USB port found..."
	exit 1
fi

stty -F $USB_PORT 115200 -icrnl ixoff -opost -isig -icanon -echo -echoe
cat $USB_PORT
