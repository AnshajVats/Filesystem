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



// Step 4: initialize the root directory on disk
int initRootDir(VCB *vcb, uint64_t blockSize) {
    // Step b: Initial number of directory entries (50)
    int numEntries = 50;
    size_t dirEntrySize = sizeof(DirectoryEntry); // Assume 60 bytes
	printf ("Size of DirectoryEntry: %zu\n", dirEntrySize);
    size_t totalBytes = numEntries * dirEntrySize; // 3000 bytes

    // Step d: Calculate blocks needed (6 blocks)
    size_t numBlocks = (totalBytes + blockSize - 1) / blockSize; // 6 blocks

    // Step e: Allocate memory for 6 blocks
    DirectoryEntry *dirEntries = malloc(numBlocks * blockSize);
    if (!dirEntries) {
        perror("Failed to allocate root directory");
        return -1;
    }
    memset(dirEntries, 0, numBlocks * blockSize);

    // Step e: Initialize all entries to "free"
    for (int i = 0; i < (numBlocks * blockSize) / dirEntrySize; i++) {
        dirEntries[i].isFree = 1;
    }

    // Step f: Get starting block (pre-reserved by initFreeSpace)
    uint64_t startBlock = vcb->rootDir;

    // Step g: Set "." entry
    strcpy(dirEntries[0].name, ".");
    dirEntries[0].isFree = 0;
    dirEntries[0].isDir = 1;
    dirEntries[0].size = (numBlocks * blockSize) - (2 * dirEntrySize); // 3060 bytes
    dirEntries[0].startBlock = startBlock;
    dirEntries[0].timestamp = time(NULL);

	printf("Root directory size: %zu bytes\n", dirEntries[0].size);

    // Step h: Set ".." entry (same as ".")
    strcpy(dirEntries[1].name, "..");
    dirEntries[1].isFree = 0;
    dirEntries[1].isDir = 1;
    dirEntries[1].size = dirEntries[0].size;
    dirEntries[1].startBlock = startBlock;
    dirEntries[1].timestamp = dirEntries[0].timestamp;

    // Step i: Write root directory to disk
    if (LBAwrite(dirEntries, numBlocks, startBlock) != numBlocks) {
        fprintf(stderr, "Failed to write root directory\n");
        free(dirEntries);
        return -1;
    }

    free(dirEntries);
    return 0;
}

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
	printf("Root directory starting at block: %zu\n", rootDirBlock);
	for (size_t i = 0; i < 6; i++) { // Reserve 6 blocks
    	freeSpace[rootDirBlock + i] = -1;
	}
	vcb->rootDir = rootDirBlock; // Starting block of root directory

	// Simply put, this is writing the free space map to disk
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
        vcb->blockSize = blockSize;
        vcb->totalBlocks = numberOfBlocks;

        // Initialize free space and root directory
        initFreeSpace(vcb, numberOfBlocks, blockSize);

		if (initRootDir(vcb, blockSize) != 0) {
            fprintf(stderr, "Failed to initialize root directory\n");
            free(vcb);
            return -1;
        }

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