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
#define EOC -2
#define INVALID_BLOCK ((uint64_t)-1)

static int* freeSpace = NULL; 
static VCB* vcb = NULL;

uint64_t allocateBlocks(int blocksNeeded) {
    uint64_t startBlock = INVALID_BLOCK;  // Using -1 consistently as not found
    uint64_t prevBlock = INVALID_BLOCK;
    int allocated = 0;
    
    // First-fit allocation
    printf("Allocating %d blocks...\n", blocksNeeded);
    printf("FAT Chain: ");
    
    for(uint64_t i = 1; i < vcb->totalBlocks && allocated < blocksNeeded; i++){
        if(freeSpace[i] == 0){ // Free block
            if(startBlock == INVALID_BLOCK) startBlock = i;
            if(prevBlock != INVALID_BLOCK) freeSpace[prevBlock] = i;
            freeSpace[i] = EOC; // Tentatively mark as allocated
            printf("%lu -> ", i);
            prevBlock = i;
            allocated++;
        }
    }
    printf("END\n");  // Add a clear ending to the chain print
    
    // Commit allocation if successful
    if(allocated == blocksNeeded) {
        freeSpace[prevBlock] = EOC; // Finalize chain
        return startBlock;
    }
    
    // Rollback on failure
    if(startBlock != INVALID_BLOCK){
        uint64_t current = startBlock;
        while(current != INVALID_BLOCK && freeSpace[current] != 0){
            uint64_t next = freeSpace[current];
            freeSpace[current] = 0;
            current = (next == EOC) ? INVALID_BLOCK : next;
        }
    }
    
    return INVALID_BLOCK;
}
int writeDir(DirectoryEntry* dir, int blocksNeeded, uint64_t startBlock) {
    return LBAwrite(dir, blocksNeeded, startBlock) == blocksNeeded ? 0 : -1;
}

DirectoryEntry* createDir(int entryCount, DirectoryEntry* parent) {
    // Calculate needed blocks
    size_t bytesNeeded = entryCount * sizeof(DirectoryEntry);
    int blocksNeeded = (bytesNeeded + vcb->blockSize - 1) / vcb->blockSize;
    
    // Allocate blocks
    uint64_t startBlock = allocateBlocks(blocksNeeded);
    if(startBlock == INVALID_BLOCK) return NULL;

	size_t fatBlocks = (vcb->totalBlocks * sizeof(int) + vcb->blockSize - 1) / vcb->blockSize;
	if (LBAwrite(freeSpace, fatBlocks, 1) != fatBlocks) {
    	printf("Failed to write FAT!\n");
    	exit(1);
	}
    
    // Initialize directory
    DirectoryEntry* dir = malloc(blocksNeeded * vcb->blockSize);
    memset(dir, 0, blocksNeeded * vcb->blockSize);
    time_t now = time(NULL);
    
    // Dot entry
    strcpy(dir[0].name, ".");
    dir[0].isDir = 1;
    dir[0].startBlock = startBlock;
    dir[0].size = bytesNeeded;  // Set directory size
    dir[0].created = dir[0].modified = now;
    
    // Dotdot entry
    strcpy(dir[1].name, "..");
    dir[1].isDir = 1;
    dir[1].startBlock = parent ? parent->startBlock : startBlock;
    dir[1].created = dir[1].modified = now;
    
    // Write to disk following chain
    uint64_t currentBlock = startBlock;
    for(int i=0; i<blocksNeeded && currentBlock != INVALID_BLOCK; i++){
        LBAwrite((char*)dir + i*vcb->blockSize, 1, currentBlock);
        currentBlock = (freeSpace[currentBlock] == EOC) ? -1 : freeSpace[currentBlock];
    }
    
    printf("Root Directory Blocks: %lu (size: %lu bytes)\n", startBlock, dir[0].size);
    printf("Dot Entry: . (Block %lu)\n", dir[0].startBlock);
    printf("DotDot Entry: .. (Block %lu)\n", dir[1].startBlock);
    
    return dir;
}

int initFreeSpace(VCB *vcb, uint64_t numberOfBlocks, uint64_t blockSize) {
	// I used calloc instead of malloc because calloc not only allocates memory 
	// but also initializes every byte to zero. 
	// If we used malloc, we would have to initialize the memory ourselves.
	// This is important because we want to ensure that the free space map is
	// initialized to zero before we start using it.
    freeSpace = calloc(numberOfBlocks, sizeof(int));
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
	printf(" Reserved Blocks (0-%zu):\n", tableBlocks);
    for (size_t i = 1; i <= tableBlocks; i++) {
        freeSpace[i] = -1;
		printf("Block %zu: %d\n", i, freeSpace[i]);
    }

	if (LBAwrite(freeSpace, tableBlocks, 1) != tableBlocks) {
        fprintf(stderr, "Failed to write free space map\n");
        free(freeSpace);
        return -1;
    }
    // Update VCB
    vcb->freeSpaceMap = 1;
    return 0;
}

int initFileSystem(uint64_t numberOfBlocks, uint64_t blockSize) {
	vcb = malloc(blockSize);

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

        DirectoryEntry* root = createDir(50, NULL);
        if (!root) {
            fprintf(stderr, "Root directory creation failed\n");
            return -1;
        }
        vcb->rootDir = root[0].startBlock;

		printf("VCB Signature: 0x%lx\n", vcb->signature);
		printf("VCB Block Size: %lu\n", vcb->blockSize);
		printf("VCB Total Blocks: %lu\n", vcb->totalBlocks);
		printf("Root Directory Start Block: %lu\n", vcb->rootDir);

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
	
	if (freeSpace) free(freeSpace);
    if (vcb) free(vcb);
	printf ("System exiting\n");
	}