Start with a new 2.6.28 kernel, copy the diff file to drivers/usb/ and run "patch -p0 < ohs900_2.6.28.diff"
The only thing you have to edit after you applyed the patch is the ohs900.h file, 
and enter the base address where the ip core lays in the addressspace. 
It can be found at the beginning of this file
