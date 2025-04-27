/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name::
* Student IDs::
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
* Project:: Basic File System
*
* File:: path.h
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

#include "mfs.h"
extern DirectoryEntry * alrLoadedRoot; // pointer for root
extern DirectoryEntry * alrLoadedcwd; 



typedef struct parsepathInfo{
	DirectoryEntry * parent; // pointer to parent directory
	int index;				 // index of entry in directory
	char * lastElement;		 // name of last pathname
} parsepathInfo;


int parsepath(char * pathname, parsepathInfo * ppI);
int isDEaDir(DirectoryEntry *de);

#endif