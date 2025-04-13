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


uint64_t AllocBlocks(int blocksNeeded) {
    return vcb->rootDir; // Assume vcb is accessible here
}

int writeDir(DirectoryEntry* dir, int blocksNeeded, uint64_t startBlock) {
    return LBAwrite(dir, blocksNeeded, startBlock) == blocksNeeded ? 0 : -1;
}

DirectoryEntry* createDir(int initialEntryCount, DirectoryEntry* parent, uint64_t blockSize) {
    // Calculate needed blocks
    size_t dirEntrySize = sizeof(DirectoryEntry);
    size_t initialBytes = initialEntryCount * dirEntrySize;
    int blocksNeeded = (initialBytes + blockSize - 1) / blockSize;
    int actualEntryCount = (blocksNeeded * blockSize) / dirEntrySize;

    // Allocate memory for directory
    DirectoryEntry* newDir = malloc(blocksNeeded * blockSize);
    if (!newDir) {
        perror("Directory allocation failed");
        return NULL;
    }
    memset(newDir, 0, blocksNeeded * blockSize);

    // Initialize all entries as unused
    for (int i = 2; i < actualEntryCount; i++) {
        newDir[i].isFree = 1;
        newDir[i].name[0] = '\0'; // Mark as unused
    }

    // Get blocks from free space system
    uint64_t startBlock = allocateBlocks(blocksNeeded, 1);
    if (startBlock == (uint64_t)-1) {
        free(newDir);
        return NULL;
    }

    time_t now = time(NULL);
    
    // Create "." entry
    strcpy(newDir[0].name, ".");
    newDir[0].isDir = 1;
    newDir[0].startBlock = startBlock;
    newDir[0].size = (actualEntryCount - 2) * dirEntrySize; // Exclude . and ..
    newDir[0].created = now;
    newDir[0].modified = now;

    // Create ".." entry (special case for root)
    strcpy(newDir[1].name, "..");
    newDir[1].isDir = 1;
    newDir[1].created = now;
    newDir[1].modified = now;

    if (parent == newDir) { // Root directory case
        newDir[1].startBlock = startBlock; // Points to self
        newDir[1].size = newDir[0].size;
    } else { // Normal directory
        newDir[1].startBlock = parent[0].startBlock;
        newDir[1].size = parent[0].size;
    }

    // Write to disk
    if (writeDir(newDir, blocksNeeded, startBlock) != 0) {
        free(newDir);
        return NULL;
    }

    return newDir;
}

int initFreeSpace(VCB *vcb, uint64_t numberOfBlocks, uint64_t blockSize) {
	// I used calloc instead of malloc because calloc not only allocates memory 
	// but also initializes every byte to zero. 
	// If we used malloc, we would have to initialize the memory ourselves.
	// This is important because we want to ensure that the free space map is
	// initialized to zero before we start using it.
    int *freeSpace = calloc(numberOfBlocks, sizeof(int));
	if (freeSpace == NULL) {
		perror("Failed to allocate memory for free space map");
		return -1;
	}
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

	if (LBAwrite(freeSpace, tableBlocks, 1) != tableBlocks) {
        fprintf(stderr, "Failed to write free space map\n");
        free(freeSpace);
        return -1;
    }

    // Allocate root directory blocks USING GLOBAL STATE
    int rootStart = allocateBlocks(freeSpace, numberOfBlocks, 6, tableBlocks + 1);
    if (rootStart == -1) {
        fprintf(stderr, "Root directory allocation failed\n");
        free(freeSpace);
        return -1;
    }

    // Update VCB
    vcb->freeSpaceMap = 1;
    vcb->rootDir = rootStart;
       // Free space starts at block 1

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

        DirectoryEntry* root = createDir(50, NULL, blockSize);
        if (!root) {
            fprintf(stderr, "Root directory creation failed\n");
            return -1;
        }
        vcb->rootDir = root[0].startBlock;

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