/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name::
* Student IDs::
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
* Project:: Basic File System
*
* File:: path.c
*
* Description:: 
*
**************************************************************/

#ifndef PATH_H
#define PATH_H

#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <dirent.h>

#include "b_io.h"
#include "mfs.h"

// int parsepath(char * pathname, parsepathInfo * ppI);

typedef struct parsepathInfo{
	DirectoryEntry * parent; // pointer to parent directory
	int index;				 // index of entry in directory
	char * lastElement;		 // name of last pathname
} parsepathInfo;

#endif