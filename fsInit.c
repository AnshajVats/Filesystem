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

#define SIGNATURE 0x40453005
#define EOC -2
#define INVALID_BLOCK ((uint64_t)-1)

static int *freeSpace = NULL;
static VCB *vcb = NULL;

// I used this function to allocate a sequence of free blocks using a simple first-fit strategy.
// It walks through the free space map and connects blocks like a linked list in FAT.
// If enough blocks aren't found, I roll back and undo the allocation.
uint64_t allocateBlocks(int blocksNeeded) {
    uint64_t first = INVALID_BLOCK;
    uint64_t last = INVALID_BLOCK;
    int count = 0;

    printf("Allocating %d blocks...\n", blocksNeeded);
    printf("FAT Chain: ");
    fflush(stdout);

    // Here I loop until I find all the blocks I need or run out of space.
    for (uint64_t i = 1; i < vcb->totalBlocks && count < blocksNeeded; i++) {
        if (freeSpace[i] == 0) {
            if (first == INVALID_BLOCK)
                first = i;
            else
                freeSpace[last] = i;

            freeSpace[i] = EOC;
            printf("%lu%s", i, (count == blocksNeeded-1) ? "" : " -> ");
            last = i;
            count++;
        }
    }

    printf("END\n");

    // If allocation worked, return the start of the chain
    if (count == blocksNeeded)
        return first;

    // If allocation failed, I undo all the changes I made above.
    uint64_t cur = first;
    while (cur != INVALID_BLOCK && freeSpace[cur] != 0) {
        uint64_t next = freeSpace[cur];
        freeSpace[cur] = 0;
        cur = (next == EOC) ? INVALID_BLOCK : next;
    }

    return INVALID_BLOCK;
}

// This function writes the directory entries to disk following the FAT-style chain.
// I also make sure to update the FAT blocks themselves with proper chaining info.
int writeDir(uint64_t start, int numBlocks, DirectoryEntry *entries) {
    uint64_t curr = start;

    // First, I write the actual contents of the directory to the disk.
    for (int i = 0; i < numBlocks && curr != INVALID_BLOCK; i++) {
        if (LBAwrite((char *)entries + (i * vcb->blockSize), 1, curr) != 1)
            return -1;
        curr = (freeSpace[curr] == EOC) ? INVALID_BLOCK : freeSpace[curr];
    }

    curr = start;

    // Now I go back and update the FAT entries that link the blocks together.
    for (int i = 0; i < numBlocks && curr != INVALID_BLOCK; i++) {
        uint64_t fatBlock = 1 + (curr / 128);
        int index = curr % 128;

        int *buf = malloc(vcb->blockSize);
        if (!buf)
            return -1;

        if (LBAread(buf, 1, fatBlock) != 1) {
            free(buf);
            return -1;
        }

        buf[index] = freeSpace[curr];  // Writing chain info into FAT

        if (LBAwrite(buf, 1, fatBlock) != 1) {
            free(buf);
            return -1;
        }

        free(buf);
        curr = (freeSpace[curr] == EOC) ? INVALID_BLOCK : freeSpace[curr];
    }

    return 0;
}

// This function creates a directory with 50 entries and inserts "." and ".." entries.
// I initialize timestamps and set up links to parent blocks to see real directory behavior.
DirectoryEntry *createDir(int entryCount, DirectoryEntry *parent) {
    size_t totalBytes = entryCount * sizeof(DirectoryEntry);
    int blockCount = (totalBytes + vcb->blockSize - 1) / vcb->blockSize;

    // I request blocks first and check if they’re available
    uint64_t firstBlock = allocateBlocks(blockCount);
    if (firstBlock == INVALID_BLOCK)
        return NULL;

    DirectoryEntry *dir = malloc(blockCount * vcb->blockSize);
    if (!dir)
        return NULL;

    memset(dir, 0, blockCount * vcb->blockSize);
    time_t t = time(NULL);

    // "." points to the directory itself
    strcpy(dir[0].name, ".");
    dir[0].isDir = 1;
    dir[0].startBlock = firstBlock;
    dir[0].size = totalBytes;
    dir[0].created = dir[0].modified = t;

    // ".." points to parent if it exists, or itself if it’s the root
    strcpy(dir[1].name, "..");
    dir[1].isDir = 1;
    dir[1].startBlock = parent ? parent->startBlock : firstBlock;
    dir[1].created = dir[1].modified = t;

    if (writeDir(firstBlock, blockCount, dir) != 0) {
        free(dir);
        return NULL;
    }

    // Just added some logging for myself to confirm where root got placed
    printf("Root Directory Blocks: %lu (size: %lu bytes)\n", firstBlock, dir[0].size);
    printf("Dot Entry: . (Block %lu)\n", dir[0].startBlock);
    printf("DotDot Entry: .. (Block %lu)\n", dir[1].startBlock);

    return dir;
}

// This function initializes the free space map and marks reserved blocks (like VCB and FAT).
// I calculate how many blocks the FAT needs based on total number of entries.
int initFreeSpace(VCB *vcb, uint64_t totalBlocks, uint64_t blockSize) {
    freeSpace = calloc(totalBlocks, sizeof(int));
    if (!freeSpace)
        return -1;

    // Block 0 is for the VCB, so I mark it as used right away
    freeSpace[0] = -1;

    size_t fatBytes = totalBlocks * sizeof(int);
    size_t fatBlocks = (fatBytes + blockSize - 1) / blockSize;

    // These are all blocks that will store the free space table itself
    printf(" Reserved Blocks (0-%zu):\n", fatBlocks);
    for (size_t i = 1; i <= fatBlocks; i++) {
        freeSpace[i] = -1;
        if(i < 5) printf("Block %zu: %d\n", i, freeSpace[i]);
    }
    printf("... (remaining blocks omitted)\n");

    // I write the initialized free space table to disk so it can persist
    if (LBAwrite(freeSpace, fatBlocks, 1) != fatBlocks) {
        free(freeSpace);
        return -1;
    }

    vcb->freeSpaceMap = 1;
    return 0;
}

// This is the entry point that sets up the whole file system (VCB, free map, root dir).
// If VCB already exists (and has a signature), I assume disk is initialized and skip setup.
int initFileSystem(uint64_t totalBlocks, uint64_t blockSize) {
    vcb = malloc(blockSize);
    if (!vcb)
        return -1;

    // I check if something is already written to the disk (block 0)
    if (LBAread(vcb, 1, 0) != 1) {
        free(vcb);
        return -1;
    }

    // If signature is missing, I reformat and start fresh
    if (vcb->signature != SIGNATURE) {
        memset(vcb, 0, blockSize);
        vcb->signature = SIGNATURE;
        vcb->blockSize = blockSize;
        vcb->totalBlocks = totalBlocks;

        // Free space map setup
        if (initFreeSpace(vcb, totalBlocks, blockSize) != 0) {
            free(vcb);
            return -1;
        }

        // Creating the actual root directory (with 50 slots)
        DirectoryEntry *root = createDir(50, NULL);
        if (!root) {
            free(vcb);
            return -1;
        }

        vcb->rootDir = root[0].startBlock;

        // Logging so I can track block sizes and addresses
        printf("VCB Signature: 0x%lx\n", vcb->signature);
        printf("VCB Block Size: %lu\n", vcb->blockSize);
        printf("VCB Total Blocks: %lu\n", vcb->totalBlocks);
        printf("Root Directory Start Block: %lu\n", vcb->rootDir);
    }

    return 0;
}

// I always free everything I malloced so there are no memory leaks.
void exitFileSystem() {
    if (freeSpace)
        free(freeSpace);
    if (vcb)
        free(vcb);
    printf("System exiting\n");
}
