/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: b_io.h
*
* Description:: Interface of basic I/O Operations
*
**************************************************************/

#ifndef _B_IO_H
#define _B_IO_H
#include <fcntl.h>
#include "mfs.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef int b_io_fd;


typedef struct b_fcb
	{
	DE * fileInfo;	//holfd information relevant to file operations
	DE * parent;	//holfd information relevant to file operations

	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
    int remainingBytes; // the number of bytes that are left in the buffer
	int buflen;		//holds how many valid bytes are in the buffer
	int currentBlock;	//holds position within file in blocks
	int numBlocks;		//holds the total number of blocks in file
    int fileIndex;      //holds the index in the parent of the file

	int activeFlags;	//holds the flags for the opened file
	} b_fcb;
	
extern b_fcb fcbArray[MAXFCBS];


extern int startup;


b_io_fd b_open (char * filename, int flags);
int b_read (b_io_fd fd, char * buffer, int count);
int b_write (b_io_fd fd, char * buffer, int count);
int b_seek (b_io_fd fd, off_t offset, int whence);
int b_close (b_io_fd fd);
void b_init ();
b_io_fd b_getFCB ();

#endif

