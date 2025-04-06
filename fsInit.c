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
#include <stdint.h>

#include "fsLow.h"
#include "mfs.h"

#define SIGNATURE 0x40453005


int initFreeSpace(VCB *vcb, uint64_t numberOfBlocks, uint64_t blockSize) {
	// I used calloc instead of malloc because calloc not only allocates memory 
	// but also initializes every byte to zero. 
	// If we used malloc, we would have to initialize the memory ourselves.
	// This is important because we want to ensure that the free space map is
	// initialized to zero before we start using it.
    int *freeSpace = calloc(numberOfBlocks, sizeof(int));
	// We set it Block 0 to -1 to indicate that it is not free.
	// block 0 is reserved for the VCB.
    freeSpace[0] = -1;

	// This will calculate the number of blocks needed for the free space map 
    size_t tableBytes = numberOfBlocks * sizeof(int);
	// this is a formula discussed in class to calculate the number of blocks needed
	// if you have some bytes then how many blocks do you need?
	// this is the same as ceil(tableBytes / blockSize)
	// or (tableBytes + blockSize - 1) / blockSize
    size_t tableBlocks = (tableBytes + blockSize - 1) / blockSize;

	// This actually sets the number of blocks to be -1
    for (size_t i = 1; i <= tableBlocks; i++) {
        freeSpace[i] = -1;
    }

	// I am also reserving some for root directory
	// It starts right after the free space map
    size_t rootDirBlock = tableBlocks + 1;
	// I set that to -1 to indicate that it is not free ( FOR STEP 4 PLEASE REVIEW THIS)
	// I kept it separate from the for loop above because I want to make sure 
	// members on step 4 can use the root directory.
    freeSpace[rootDirBlock] = -1;


    if (LBAwrite(freeSpace, tableBlocks, 1) != tableBlocks) {
		fprintf(stderr, "Error writing free space map to disk\n");
		free(freeSpace);
		return -1;
	}

    vcb->freeSpaceMap = 1;          // Free space starts at block 1
    vcb->rootDir = rootDirBlock;    // Root directory starts after free space

    free(freeSpace);
    return 0;
}

int initFileSystem(uint64_t numberOfBlocks, uint64_t blockSize) {
    // This is doing step 1
	VCB *vcb = malloc(blockSize);

	if (vcb == NULL) {
		perror("Failed to allocate memory for VCB");
		return -1;
	}
	if (LBAread(vcb, 1, 0) != 1) {
    	fprintf(stderr, "Error reading VCB from disk\n");
    	free(vcb);
    	exit(1);
	}

    if(vcb->signature != SIGNATURE) {

        memset(vcb, 0, blockSize);

        vcb->signature = SIGNATURE;
        vcb->volumeSize = numberOfBlocks * blockSize;
        vcb->totalBlocks = numberOfBlocks;

        // Initialize free space and root directory
        initFreeSpace(vcb, numberOfBlocks, blockSize);
        //initRootDir(vcb->rootDir, blockSize);

        // Write VCB to disk
        if (LBAwrite(vcb, 1, 0) != 1) {
       		fprintf(stderr, "Error writing VCB to disk\n");
        	free(vcb);
        	exit(1);
    	}
    }
    return 0;
}
void exitFileSystem ()
	{
	printf ("System exiting\n");
	}