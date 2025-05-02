/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name:: Karina Alvarado Mendoza
* Student IDs:: 921299233
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
* Project:: Basic File System
*
* File:: b_io.c
*
* Description:: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include <path.h>
#define PATH_NO_VALUE_LEFT -2


#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
	int buflen;		//holds how many valid bytes are in the buffer
	int flags;
	parsepathInfo *pathInfo;
	DirectoryEntry * fileEntry;
	


	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buf == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open (char * filename, int flags)
	{
	//validate the access mode
	if ((flags & O_ACCMODE) == O_ACCMODE)
	{
		return -1;
	}	
	//int parsepath(char * pathname, parsepathInfo * ppI) {
	parsepathInfo* pathInfo = (parsepathInfo*)malloc(sizeof(parsepathInfo));
    if (!pathInfo || parsepath(filename, pathInfo) != 0 || pathInfo->lastElement == PATH_NO_VALUE_LEFT)
    {
        free(pathInfo);
        return -1;
    }

    if (pathInfo->lastElement == -1)
    {
        if ((flags & O_CREAT) != O_CREAT || createFile(pathInfo) == -1)
        {
            freeParsedpathInfo(pathInfo);
            free(pathInfo);
            return -1;
        }
        pathInfo->lastElement = LocateEntry(pathInfo->parent);
    }
    //checking if the index we are currently in is a directory
    if (isDir(&pathInfo->parent[pathInfo->index]))
    {
        freeParsedpathInfo(pathInfo);
        free(pathInfo);
        return -1;
    }

    if ((flags & O_TRUNC) == O_TRUNC && truncatedOfFile(pathInfo) == -1)
    {
        freeParsedpathInfo(pathInfo);
        free(pathInfo);
        return -1;
    }

    if (startup == 0)
        b_init();

    b_io_fd returnFd = b_getFCB();
    if (returnFd == -1)
    {
        freeParsedpathInfo(pathInfo);
        free(pathInfo);
        return -1;
    }

    fcbArray[returnFd].buf = (char*)malloc(B_CHUNK_SIZE);
    if (!fcbArray[returnFd].buf)
    {
        freeParsedpathInfo(pathInfo);
        free(pathInfo);
        return -1;
    }

	b_io_fd returnFd;
	}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
		
	return (0); //Change this
	}



// Interface to write function	
int b_write (b_io_fd fd, char * buffer, int count)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
		
	return (0); //Change this
	}



// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
	{

	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
	return (0);	//Change this
	}
	
// Interface to Close the file
int b_close(b_io_fd fd)
{
    // Validate the file descriptor
    if (fd < 0 || fd >= MAXFCBS || fcbArray[fd].buf == NULL)
    {
        return -1;
    }

    b_fcb* fcb = &fcbArray[fd];

    // Write back dirty data if the file is not read-only
    if ((fcb->flags & O_ACCMODE) != O_RDONLY)
    {
        flushBuffer(fcb);

        // Handle empty file cleanup
        if (fcb->fileEntry->size == 0 && fcb->fileEntry->startBlock > 0)
        {
            freeBlocks(fcb->fileEntry->startBlock);
        }
        else
        {
            changeBlocksAllocate(fcb->fileEntry->startBlock,
                                 sizeToBlocks(fcb->fileEntry->size, B_CHUNK_SIZE));
        }
    }

    // Free resources associated with the file
    freeParsedpathInfo(fcb->pathInfo);
    free(fcb->pathInfo);
    fcb->pathInfo = NULL;
    fcb->fileEntry = NULL;

    free(fcb->buf);
    fcb->buf = NULL;

    return 0;
}
