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




#define SIGNATURE 0x40453005  // We use this as a magic number to validate the filesystem
#define EOC -2                // Represents the end of a FAT chain
#define INVALID_BLOCK ((uint64_t)-1)  // Used when no valid block can be found

static int *freeSpace = NULL;  // This points to our in-memory free space bitmap or FAT
 VCB *vcb = NULL;        // This points to the volume control block (VCB) structure
DirectoryEntry *alrLoadedRoot = NULL;
DirectoryEntry *alrLoadedcwd = NULL;

// We use this function to allocate a specified number of blocks.
// It links them together in a FAT-like chain and returns the starting block.
// If not enough space is found, we roll back the allocation.
uint64_t allocateBlocks(int blocksNeeded) {
    uint64_t first = INVALID_BLOCK;
    uint64_t last = INVALID_BLOCK;
    int count = 0;

    printf("Allocating %d blocks...\n", blocksNeeded);
    printf("FAT Chain: ");
    fflush(stdout);

    // We go through the entire free space array to find unused blocks.
    for (uint64_t i = 1; i < vcb->totalBlocks && count < blocksNeeded; i++) {
        if (freeSpace[i] == 0) {
            if (first == INVALID_BLOCK)
                first = i;  // This becomes the start of our chain
            else
                freeSpace[last] = i;  // We link previous block to the current

            freeSpace[i] = EOC;  // We tentatively mark this as the end of chain
            printf("%lu%s", i, (count == blocksNeeded-1) ? "" : " -> ");
            last = i;
            count++;
        }
    }

    printf("END\n");

    // If we managed to allocate all blocks, return the start block
    if (count == blocksNeeded)
        return first;

    // Otherwise, we clean up what we allocated so far (rollback)
    uint64_t cur = first;
    while (cur != INVALID_BLOCK && freeSpace[cur] != 0) {
        uint64_t next = freeSpace[cur];
        freeSpace[cur] = 0;
        cur = (next == EOC) ? INVALID_BLOCK : next;
    }

    return INVALID_BLOCK;
}

// This function writes directory entries to disk, one block at a time.
// It uses the FAT chain to traverse and write blocks, and then updates the FAT itself.
int writeDir(uint64_t start, int numBlocks, DirectoryEntry *entries) {
    uint64_t curr = start;

    // First we write the directory content to disk block by block
    for (int i = 0; i < numBlocks && curr != INVALID_BLOCK; i++) {
        if (LBAwrite((char *)entries + (i * vcb->blockSize), 1, curr) != 1)
            return -1;
        curr = (freeSpace[curr] == EOC) ? INVALID_BLOCK : freeSpace[curr];
    }

    curr = start;

    // Now we update the actual FAT entries that describe the block chain
    for (int i = 0; i < numBlocks && curr != INVALID_BLOCK; i++) {
        uint64_t fatBlock = 1 + (curr / 128);  // Figure out which FAT block to write to
        int index = curr % 128;               // Find the index inside that FAT block

        int *buf = malloc(vcb->blockSize);
        if (!buf)
            return -1;

        if (LBAread(buf, 1, fatBlock) != 1) {  // Read existing FAT block
            free(buf);
            return -1;
        }

        buf[index] = freeSpace[curr];  // Update the FAT entry with the next block in chain

        if (LBAwrite(buf, 1, fatBlock) != 1) {  // Write updated FAT block back to disk
            free(buf);
            return -1;
        }

        free(buf);
        curr = (freeSpace[curr] == EOC) ? INVALID_BLOCK : freeSpace[curr];
    }

    return 0;
}

// This function creates a directory and populates it with . and .. entries.
// It allocates enough space for the directory and writes it to disk.
DirectoryEntry *createDir(int entryCount, DirectoryEntry *parent) {
    size_t totalBytes = entryCount * sizeof(DirectoryEntry);
    int blockCount = (totalBytes + vcb->blockSize - 1) / vcb->blockSize;

    // We get a chain of blocks for our directory
    uint64_t firstBlock = allocateBlocks(blockCount);
    if (firstBlock == INVALID_BLOCK)
        return NULL;

    DirectoryEntry *dir = malloc(blockCount * vcb->blockSize);
    if (!dir)
        return NULL;

    memset(dir, 0, blockCount * vcb->blockSize);
    time_t t = time(NULL);

    // The "." entry always points to the current directory
    strcpy(dir[0].name, ".");
    dir[0].isDir = 1;
    dir[0].startBlock = firstBlock;
    dir[0].size = blockCount * vcb->blockSize;
    dir[0].created = dir[0].modified = t;

    // The ".." entry points to parent or to self if this is the root
    strcpy(dir[1].name, "..");
    dir[1].isDir = 1;
    dir[1].startBlock = parent ? parent->startBlock : firstBlock;
    dir[1].created = dir[1].modified = t;

    if (writeDir(firstBlock, blockCount, dir) != 0) {
        free(dir);
        return NULL;
    }

    printf("[createDir] Created directory '%s' at block %lu (parent: %lu)\n", 
       dir[0].name, firstBlock, parent ? parent->startBlock : 0);

    printf("Dot Entry: . (Block %lu)\n", dir[0].startBlock);
    printf("DotDot Entry: .. (Block %lu)\n", dir[1].startBlock);

    return dir;
}

// This sets up the free space structure, which tracks block usage using a FAT-style table.
// We also mark system-reserved blocks (like the VCB and FAT itself).
int initFreeSpace(VCB *vcb, uint64_t totalBlocks, uint64_t blockSize) {
    freeSpace = calloc(totalBlocks, sizeof(int));
    if (!freeSpace)
        return -1;

    freeSpace[0] = -1;  // Block 0 is reserved for the VCB

    size_t fatBytes = totalBlocks * sizeof(int);
    size_t fatBlocks = (fatBytes + blockSize - 1) / blockSize;  // How many blocks FAT takes

    // Mark the FAT blocks as used so they aren't reused later
    printf(" Reserved Blocks (0-%zu):\n", fatBlocks);
    for (size_t i = 1; i <= fatBlocks; i++) {
        freeSpace[i] = -1;
        if(i < 5) printf("Block %zu: %d\n", i, freeSpace[i]);
    }
    printf("... (remaining blocks omitted)\n");

    if (LBAwrite(freeSpace, fatBlocks, 1) != fatBlocks) {
        free(freeSpace);
        return -1;
    }

    vcb->freeSpaceMap = 1;  // This tells us where the FAT begins
    return 0;
}

// This is the main initialization routine that prepares the file system on first use.
// If a valid signature is already present, we skip reinitialization.
int initFileSystem(uint64_t totalBlocks, uint64_t blockSize) {
    vcb = malloc(blockSize);
    if (!vcb)
        return -1;

    // We check if the VCB already exists on disk
    if (LBAread(vcb, 1, 0) != 1) {
        free(vcb);
        return -1;
    }

    if (vcb->signature != SIGNATURE) {
        // If not, we assume this is a new disk and set everything up from scratch
        memset(vcb, 0, blockSize);
        vcb->signature = SIGNATURE;
        vcb->blockSize = blockSize;
        vcb->totalBlocks = totalBlocks;

        // Initialize free space tracking
        if (initFreeSpace(vcb, totalBlocks, blockSize) != 0) {
            free(vcb);
            return -1;
        }

        // Create and write the root directory
        DirectoryEntry *root = createDir(50, NULL);
        if (!root) {
            free(vcb);
            return -1;
        }

        vcb->rootDir = root[0].startBlock;
        alrLoadedRoot = root; // Assign root directory to global pointer
        alrLoadedcwd = root;
        if (LBAwrite(vcb, 1, 0) != 1) {
            perror("Failed to write VCB to disk");
            free(vcb);
            return -1;
        }


        // Log some useful values for verification
        printf("VCB Signature: 0x%lx\n", vcb->signature);
        printf("VCB Block Size: %lu\n", vcb->blockSize);
        printf("VCB Total Blocks: %lu\n", vcb->totalBlocks);
        printf("Root Directory Start Block: %lu\n", vcb->rootDir);
    } 
    else {
        // Existing file system: Load FAT and root directory

        // Load FAT into freeSpace
        size_t fatBytes = vcb->totalBlocks * sizeof(int);
        size_t fatBlocks = (fatBytes + vcb->blockSize - 1) / vcb->blockSize;

        freeSpace = malloc(fatBlocks * vcb->blockSize);
        if (!freeSpace) {
            free(vcb);
            return -1;
        }

        if (LBAread(freeSpace, fatBlocks, vcb->freeSpaceMap) != fatBlocks) {
            free(freeSpace);
            free(vcb);
            return -1;
        }

        // Load root directory
        uint64_t currentBlock = vcb->rootDir;
        int blockCount = 0;

        // Calculate the number of blocks in the root directory's chain
        while (currentBlock != EOC && currentBlock != INVALID_BLOCK) {
            blockCount++;
            int nextBlock = freeSpace[currentBlock];
            currentBlock = (nextBlock == EOC) ? INVALID_BLOCK : nextBlock;
        }

        DirectoryEntry *root = malloc(blockCount * vcb->blockSize);
        if (!root) {
            free(freeSpace);
            free(vcb);
            return -1;
        }

        currentBlock = vcb->rootDir;
        int blocksRead = 0;
        while (currentBlock != INVALID_BLOCK && blocksRead < blockCount) {
            if (LBAread((char *)root + (blocksRead * vcb->blockSize), 1, currentBlock) != 1) {
                free(root);
                free(freeSpace);
                free(vcb);
                return -1;
            }
            blocksRead++;
            int nextBlock = freeSpace[currentBlock];
            currentBlock = (nextBlock == EOC) ? INVALID_BLOCK : nextBlock;
        }

        alrLoadedRoot = root;
        alrLoadedcwd = root;
        printf("Loaded existing root directory from block %lu\n", vcb->rootDir);
    }

    return 0;
}

// Cleanup function to release allocated memory on exit.
void exitFileSystem() {
    if (freeSpace)
        free(freeSpace);
    if (vcb)
        free(vcb);
    if (alrLoadedRoot) free(alrLoadedRoot); // Add
    if (alrLoadedcwd) free(alrLoadedcwd);   // Add
    printf("System exiting\n");
}
