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
#include "b_io.h"

DE* loadDir(DE* searchDirectory, int index);
int parsePath(const char* pathName, PPRETDATA* ppinfo);
int findInDir(DE* searchDirectory, char* name);
int find_vacant_space (DE * directory , char * fileName);


// helper functions 

b_io_fd initializeFCB();
PPRETDATA* parseFilePath(char *filename, DE **fileInfo);
int handleReadMode(b_io_fd fd, PPRETDATA *ppinfo);
int handleWriteMode(b_io_fd fd, PPRETDATA *ppinfo, int flags);
int handleCreateMode(b_io_fd fd, PPRETDATA *ppinfo);
void setupCommonFCBFields(b_io_fd fd, DE *parent, int flags);
void updateParentDirectory(b_io_fd fd);
void cleanupResources(PPRETDATA *ppinfo, DE *fileInfo, b_fcb *fcb);

#endif