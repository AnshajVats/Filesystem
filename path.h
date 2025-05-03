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
#include "fsInit.h"

DE* loadDir(DE* searchDirectory, int index);
int parsePath(const char* pathName, PPRETDATA* ppinfo);
int findInDir(DE* searchDirectory, char* name);
int find_vacant_space (DE * directory , char * fileName);

#endif