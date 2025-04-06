/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: fsInit.c
*
* Description:: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "mfs.h"

#define SIGNATURE 0x40453005

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
	{
	printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
	/* TODO: Add any code you need to initialize your file system. */
	
	VCB *vcb = malloc(blockSize);
	LBAread(vcb, 1, 0);

	if(vcb->signature != SIGNATURE){
		// initializing volume

		// referencing VCB variables
		vcb->signature = SIGNATURE;
		vcb->volumeSize = numberOfBlocks;
		vcb->totalBlocks = blockSize;

		vcb->freeSpaceMap; // set equal to function of freespace
		vcb->rootDir; // set equal to function of root Directory

		LBAwrite(vcb, 1, 0);
	} else{
		// volume already initialized
		printf("Volume already initailized!");
	}
	return 0;
	}
	
	
void exitFileSystem ()
	{
	printf ("System exiting\n");
	}