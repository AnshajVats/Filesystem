/**************************************************************
* Class::  CSC-415-02 Spring 2025
* Name::
* Student IDs::
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
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
    vcb->freeSpaceMap = 1;
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
        return -1; // invalid number of blocks requested
    }
    if( numberOfBlocks > vcb->totalFreeSpace ) {
        return -1; // not enough free space
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
    // updating free block to next
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

DirectorySizeInfo calculateDirectorySize(int numberOfEntries) {
    DirectorySizeInfo info;
    info.bytesNeeded = sizeof(DE) * numberOfEntries;
    info.blockCount = calculateFormula(info.bytesNeeded, MINBLOCKSIZE);
    info.maxEntryCount = (info.blockCount * MINBLOCKSIZE) / sizeof(DE);
    return info;
}

DE* createDirectoryBuffer(int blockCount, int maxEntries) {
    DE* buffer = malloc(blockCount * MINBLOCKSIZE);
    if (!buffer) {
        perror("Directory buffer allocation failed");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 0, blockCount * MINBLOCKSIZE);
    
    // Initialize entries to unused state
    for (int i = 0; i < maxEntries; i++) {
        buffer[i].location = -2;
    }
    return buffer;
}

void initializeCurrentDirEntry(DE* entry, int blockLocation, int dirSize) {
    entry->location = blockLocation;
    entry->size = dirSize;
    entry->isDirectory = 1;
    strncpy(entry->name, ".", DE_NAME_SIZE);
    
    time_t now = time(NULL);
    entry->dateCreated = now;
    entry->dateModified = now;
    entry->dateLastAccessed = now;
}

void initializeParentDirEntry(DE* entry, const DE* parent) {
    strncpy(entry->name, "..", DE_NAME_SIZE);
    entry->isDirectory = 1;
    
    if (parent) {
        entry->location = parent->location;
        entry->size = parent->size;
    }
}

void handleRootDirectory(VCB* vcb, DE* buffer, int blockCount, int blockLocation) {
    printf("Initializing root directory\n");
    vcb->rootSize = blockCount;
    vcb->rootDir = blockLocation;
    buffer[1] = buffer[0];  // Copy . entry to .. for root
}

// Main function
int createDirectory(int numberOfEntries, DE* parent) {
    // Calculate size requirements
    DirectorySizeInfo sizeInfo = calculateDirectorySize(numberOfEntries);
    
    // Allocate memory buffer
    DE* dirBuffer = createDirectoryBuffer(sizeInfo.blockCount, sizeInfo.maxEntryCount);
    
    // Get block allocation
    int blocksRequested = allocateblock(sizeInfo.blockCount);
    
    // Initialize directory entries
    initializeCurrentDirEntry(&dirBuffer[0], blocksRequested, 
                            sizeInfo.maxEntryCount * sizeof(DE));
    
    if (parent) {
        initializeParentDirEntry(&dirBuffer[1], parent);
    } else {
        handleRootDirectory(vcb, dirBuffer, sizeInfo.blockCount, blocksRequested);
    }

    // Write to disk
    int blocksWritten = fileWrite(dirBuffer, sizeInfo.blockCount, blocksRequested);
    
    // Cleanup resources
    free(dirBuffer);
    return blocksRequested;
}

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize){
	VCB * 		buffer;

    int blocksNeeded = calculateFormula(sizeof(int)*numberOfBlocks, blockSize);
    freeSpaceMap = (int *) malloc(blocksNeeded * blockSize );
	vcb = (VCB *) malloc(MINBLOCKSIZE);
	root = (DE *) malloc(DE_SIZE);
	cwdPathName = (char *) malloc(512);

	buffer = (VCB *) malloc(MINBLOCKSIZE);
	LBAread ( buffer, 1, 0);

	if ( buffer->signature != SIGNATURE ){
        printf("Formatting disk\n");
		memset(vcb, 0, MINBLOCKSIZE);
		vcb -> signature = SIGNATURE;
		vcb -> totalBlocks = numberOfBlocks;
		vcb -> blockSize = blockSize;
		vcb -> firstBlock =initFreespace(numberOfBlocks, MINBLOCKSIZE);
		printf("Free space initialized\n");
		vcb -> freeSpaceMap = 1;
		vcb -> rootDir =createDirectory(50, NULL);
		LBAwrite(vcb, 1, 0);
		LBAread ( root, vcb->rootSize, vcb->rootDir );
	}else{
        LBAread(vcb, 1, 0);
		printf("Disk already formatted\n");
		LBAread ( root, vcb->rootSize, vcb->rootDir );
		LBAread ( freeSpaceMap, vcb->freeSpaceSize, vcb->freeSpaceMap);

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
		vcb -> freeSpaceMap);
    free(freeSpaceMap);
    free(vcb);
    free(root);
    free(cwdPathName);
    free(cwd);
	printf ("System exiting\n");
}