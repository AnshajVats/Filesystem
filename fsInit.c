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
    int *freeSpace = calloc(numberOfBlocks, sizeof(int));
    freeSpace[0] = -1;

    size_t tableBytes = numberOfBlocks * sizeof(int);
    size_t tableBlocks = (tableBytes + blockSize - 1) / blockSize;

    for (size_t i = 1; i <= tableBlocks; i++) {
        freeSpace[i] = -1;
    }

    size_t rootDirBlock = tableBlocks + 1;
    freeSpace[rootDirBlock] = -1;

    uint8_t *tableBuffer = calloc(tableBlocks, blockSize);
    memcpy(tableBuffer, freeSpace, tableBytes);
    LBAwrite(tableBuffer, tableBlocks, 1);

    vcb->freeSpaceMap = 1;          // Free space starts at block 1
    vcb->rootDir = rootDirBlock;    // Root directory starts after free space

    free(tableBuffer);
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