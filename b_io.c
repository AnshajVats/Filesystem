/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
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

#include "path.h"
#include "fsInit.h"
#include "mfs.h"
#include "fsLow.h"


b_fcb fcbArray[MAXFCBS]; 
int startup = 0;  

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
b_io_fd b_open(char *filename, int flags) {
    b_io_fd returnFd = -1;
    PathParseResult *ppinfo = NULL;
    DE *fileInfo = NULL;
    b_fcb *fcb = NULL;

    // Initialize system and get FCB
    if ((returnFd = initializeFCB()) < 0) {
        return returnFd;
    }
    fcb = &fcbArray[returnFd];

    // Parse file path and retrieve directory info
    if (!(ppinfo = parseFilePath(filename, &fileInfo))) {
        fprintf(stderr, "Error parsing path\n");
        cleanupResources(NULL, NULL, fcb);
        return -1;
    }

    // Determine and handle the open mode
    int modeHandled = 0;
    if ((flags & O_RDONLY) && ppinfo->lastElementIndex >= 0) {
        modeHandled = handleReadMode(returnFd, ppinfo);
    } else if (ppinfo->lastElementIndex >= 0) {
        modeHandled = handleWriteMode(returnFd, ppinfo, flags);
    } else if (flags & O_CREAT) {
        modeHandled = handleCreateMode(returnFd, ppinfo);
    }

    if (!modeHandled) {
        cleanupResources(ppinfo, fileInfo, fcb);
        return -1;
    }

    // Setup common FCB fields and copy parent directory data
    DE *parentCopy = malloc(DE_SIZE);
    if (!parentCopy) {
        cleanupResources(ppinfo, fileInfo, fcb);
        return -1;
    }
    memcpy(parentCopy, ppinfo->parent, DE_SIZE);
    setupCommonFCBFields(returnFd, parentCopy, flags);

    // Update parent directory on disk
    updateParentDirectory(returnFd);

    // Cleanup parsed resources
    cleanupResources(ppinfo, fileInfo, NULL);

    return returnFd;
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
int b_close (b_io_fd fd)
	{

	}