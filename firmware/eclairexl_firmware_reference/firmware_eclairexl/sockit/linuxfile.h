#pragma once

#include "simplefile.h"

#include <stdio.h>

struct SimpleFile
{
	FILE * file;
	char path[256];
	char name[256];
};

