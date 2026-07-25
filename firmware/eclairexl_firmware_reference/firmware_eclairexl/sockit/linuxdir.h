#pragma once

#include "simpledir.h"

// Extends simple dir with way of opening files and looking at dirs!
// Not all systems provide this...

#include <dirent.h>

struct SimpleDirEntry
{
        struct dirent entry;
	char path[256];
	struct SimpleDirEntry * next;
};


