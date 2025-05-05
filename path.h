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
int parsePath(const char* pathName, PathParseResult* ppinfo);
int findInDir(DE* searchDirectory, char* name);
int find_vacant_space (DE * directory , char * fileName);


// helper functions 

b_io_fd initializeFCB();
PathParseResult* parseFilePath(char *filename, DE **fileInfo);
int handleReadMode(b_io_fd fd,PathParseResult *ppinfo);
int handleWriteMode(b_io_fd fd,PathParseResult *ppinfo, int flags);
int handleCreateMode(b_io_fd fd, PathParseResult *ppinfo);
void setupCommonFCBFields(b_io_fd fd, DE *parent, int flags);
void updateParentDirectory(b_io_fd fd);
void cleanupResources(PathParseResult *ppinfo, DE *fileInfo, b_fcb *fcb);

int perform_write(b_fcb *fcb, char *data, int length);
int ensure_space(b_fcb *fcb, int required_bytes);
int validate_write(b_io_fd fd, int count);

int validate_read(b_io_fd fd, int count, b_fcb **fcb_ptr);
void calculate_segments(b_fcb *fcb, int count, 
                              int *part1, int *part2, int *part3);

int perform_write(b_fcb *fcb, char *data, int length);
int ensure_space(b_fcb *fcb, int required_bytes);
int validate_write(b_io_fd fd, int count);

int validate_read(b_io_fd fd, int count, b_fcb **fcb_ptr);
void calculate_segments(b_fcb *fcb, int count, 
                              int *part1, int *part2, int *part3);

#endif