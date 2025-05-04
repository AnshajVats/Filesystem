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
* This file is where you will start and initialize your system.
*
**************************************************************/


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "fsLow.h"
#include "mfs.h"
#include "fsInit.h"



#define SIGNATURE 0x40453005
#define FS_BLOCK_END_OF_CHAIN 0xFFFFFFFF


VCB * vcb;
int * freeSpaceMap;
DE * root;
DE * cwd;
char * cwdPathName;

int initFreespace(uint64_t numberOfBlocks, uint64_t blockSize){
    // rounding for blocks: (n + m - 1 )/ m
    // the number of blocks the freespace map needs
    int blocksNeeded = calculateFormula(sizeof(int)*numberOfBlocks, blockSize);
    // int* freeSpaceMap = malloc( blocksNeeded * blockSize );
    for( int i = 1; i < numberOfBlocks; i++ ) {
        freeSpaceMap[i] = i+1;
    }

    // mark the VCB as used
    freeSpaceMap[0] = 0xFFFFFFFF;
    // mark the freespace map as used
    freeSpaceMap[blocksNeeded] = 0xFFFFFFFF;
    freeSpaceMap[numberOfBlocks] = 0xFFFFFFFF;

    int blocksWritten = LBAwrite(freeSpaceMap, blocksNeeded, 1);
    vcb->totalFreeSpace = numberOfBlocks - blocksNeeded;
    vcb->freeSpaceLocation = 1;
    vcb->freeSpaceSize = blocksNeeded;
    return blocksWritten == -1 ? -1: blocksNeeded + 1;
}

/*
 * get free blocks
 *
 * @param numberOfBlocks the number of blocks that are needed
 * @return the location of the first block that can be used. -1 on error
 */
int allocateblock(uint64_t numberOfBlocks) {
    if( numberOfBlocks < 1 ) {
        return -1;
    }
    if( numberOfBlocks > vcb->totalFreeSpace ) {
        return -1;
    }

    // first free block in the freeSpaceMap table
    int head = vcb->firstBlock;
    int currBlockLoc = vcb->firstBlock;
    int nextBlockLoc = freeSpaceMap[currBlockLoc];
    vcb->totalFreeSpace--;
    // jump through the blocks to find what the new first block will be
    for( int i = 1; i < numberOfBlocks; i ++ ) {
        currBlockLoc = nextBlockLoc;
        nextBlockLoc = freeSpaceMap[currBlockLoc];
        vcb->totalFreeSpace--;
    }
    freeSpaceMap[currBlockLoc] = 0xFFFFFFFF;
    vcb->firstBlock = nextBlockLoc;
    return head;
}

/*
 * return free blocks
 *
 * @param location the location of the block for the blocks being returned
 * @return the number of blocks that were returned. -1 on error
 */
int returnFreeBlocks(int location){
    if(location < 1 || location > vcb->totalBlocks) {
        fprintf(stderr, "Invalid location\n");
        return -1;
    }
    
    int currBlockLoc = location;
    int blocksReturned = 0;
    
    // Count blocks in chain
    while(freeSpaceMap[currBlockLoc] != FS_BLOCK_END_OF_CHAIN) {
        currBlockLoc = freeSpaceMap[currBlockLoc];
        blocksReturned++;
    }
    
    // Link returned chain to front of free list
    freeSpaceMap[currBlockLoc] = vcb->firstBlock;
    vcb->firstBlock = location;
    return blocksReturned;
}
/*
 * write blocks to disk
 *
 * @param buff the buffer that is being written
 * @param numberOfBlocks the number of blocks being written
 * @param location the location where the blocks are written
 * @return the number of blocks written
 */
int fileWrite(void* buff, int numberOfBlocks, int location){
    int blockSize = vcb->blockSize;
    int blocksWritten = 0;
    for( int i = 0; location != -1l && i < numberOfBlocks; i++ ) {
        if( LBAwrite(buff + blockSize * i, 1, location) == -1 ) {
            return -1;
        }
        location = freeSpaceMap[location];
        blocksWritten++;
    }
    return blocksWritten;
}

/*
 * read blocks from the disk
 *
 * @param buff the buffer that is being filled
 * @param numberOfBlocks the number of blocks being read
 * @param location the location where the blocks are read from
 * @return the number of blocks read
 */
int fileRead(void* buff, int numberOfBlocks, int location){
    int blockSize = vcb->blockSize;
    int blocksRead = 0;
    for( int i = 0; location != -1l && i < numberOfBlocks; i++ ) {
        if( LBAread(buff + blockSize*i, 1, location) == -1) {
            return -1;
        }
        location = freeSpaceMap[location];
        blocksRead++;
    }
    return blocksRead;
}

/*
 * get the index n blocks over
 *
 * @param location the location of the block where the search is starting at
 * @param numberOfBlocks the number of blocks to move over
 * @return the index of the block n blocks over. -1 on error
 */
int fileSeek(int location, int numberOfBlocks){
    for( int i = 0; location != -1l && i < numberOfBlocks; i ++ ) {
        location = freeSpaceMap[location];
    }
    return location;
}

int createDirectory(int numberOfEntries, DE *parent)
{
	int bytes;		// The number of bytes needed for entries
	int blockCount;		// The total number of blocks needed
	int maxEntryCount;	// How many entries we can fit into our blocks
	int blocksRequested; 	// Location of first block for new directory

	/* From # of entries argument determine the maximum number of
	 * entries we can actually fit */
	bytes = sizeof(DE) * numberOfEntries;
	blockCount = calculateFormula(bytes, MINBLOCKSIZE);
		// ((bytes + MINBLOCKSIZE - 1) / MINBLOCKSIZE);
	maxEntryCount = (blockCount * MINBLOCKSIZE) / sizeof(DE);

	// Allocate memory for directory
	DE *buffer = malloc(blockCount * MINBLOCKSIZE);
	memset(buffer, 0, blockCount *  MINBLOCKSIZE);
	if (buffer == NULL)
	{
		printf("Error: Could not allocate memory for new directory\n");
		exit(EXIT_FAILURE);
	}

	// Initialize each directory entry in new directory to an unused state
	for (int i = 0; i < maxEntryCount; i++)
		buffer[i].location = -2;

	// Request blocks from freespace system
	blocksRequested = allocateblock(blockCount);
	// printf("Creating Directory at %d, with size %ld bytes\n", blocksRequested, maxEntryCount * sizeof(DE));

	// Initialize dot and dot dot entries of the new directory
	buffer[0].location = blocksRequested;
	buffer[0].size = maxEntryCount * sizeof(DE);
	buffer[0].isDirectory = 1;
	strncpy(buffer[0].name, ".", DE_NAME_SIZE);

	// Set timestamp fields
	time_t tm = time(NULL);
	buffer[0].dateCreated = tm;
	buffer[0].dateModified = tm;
	buffer[0].dateLastAccessed = tm;

	// If no parent is passed, initialize root directory 
	// Cannot handle if root already exists
	if (parent == NULL)
	{
		printf("Initializing root directory\n");
		vcb->rootSize = blockCount;
		vcb->rootLocation = blocksRequested;
		buffer[1] = buffer[0];

		/* If a parent directory entry is provided, initialize new directory's
		 * parent and link the new directory entry back to the parent */
	}
	else
	{
		buffer[1].location = parent -> location;
		buffer[1].size = parent -> size;
	}
	strncpy(buffer[1].name, "..", DE_NAME_SIZE);
	buffer[1].isDirectory = 1;

	// Write newly created directory to disk
	int blocksWritten = fileWrite(buffer, blockCount, blocksRequested);
    free(buffer);
	return blocksRequested;
}

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize){
	VCB * 		buffer;

    int blocksNeeded = calculateFormula(sizeof(int)*numberOfBlocks, blockSize);
    freeSpaceMap = (int *) malloc(blocksNeeded * blockSize );
	vcb = (VCB *) malloc(MINBLOCKSIZE);
	root = (DE *) malloc(DE_SIZE);
	cwdPathName = (char *) malloc(512);

	printf ("Initializing File System with %ld blocks \
		with a block size of %ld\n", numberOfBlocks, blockSize);

	buffer = (VCB *) malloc(MINBLOCKSIZE);
	LBAread ( buffer, 1, 0);

	if ( buffer->signature == SIGNATURE ){
        	LBAread(vcb, 1, 0);
		printf("Disk already formatted\n");
		LBAread ( root, vcb->rootSize, vcb->rootLocation );
		LBAread ( freeSpaceMap, vcb->freeSpaceSize,
			vcb->freeSpaceLocation);
	}else{
		printf("Formatting disk\n");
		memset(vcb, 0, MINBLOCKSIZE);
		vcb -> signature = SIGNATURE;
		vcb -> totalBlocks = numberOfBlocks;
		vcb -> blockSize = blockSize;
		vcb -> firstBlock =
			initFreespace(numberOfBlocks, MINBLOCKSIZE);
		printf("Free space initialized\n");
		vcb -> freeSpaceLocation = 1;
		vcb -> rootLocation =
			createDirectory(50, NULL);
		LBAwrite(vcb, 1, 0);

		LBAread ( root, vcb->rootSize, vcb->rootLocation );
	}
	fs_setcwd("/");
	strncpy(cwdPathName, "/", 36);

	free(buffer);

	return 0;
}

void exitFileSystem (){
	fileWrite(vcb, 1, 0);
	fileWrite(freeSpaceMap,
		MINBLOCKSIZE * vcb -> freeSpaceSize,
		vcb -> freeSpaceLocation);
    free(freeSpaceMap);
    free(vcb);
    free(root);
    free(cwdPathName);
    free(cwd);
	printf ("System exiting\n");
}