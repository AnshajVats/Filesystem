/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name::
* Student IDs::
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
* Project:: Basic File System
*
* File:: mfs.c
*
* Description:: 
*
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include "mfs.h"
#include "fsLow.h"
#include "path.h"
#include "fsInit.h"

#define INVALID_BLOCK ((uint64_t)-1)  // Used to track if allocation failed
#define EOC -2  // We define end of chain for FAT-like setup, useful for debugging later

extern VCB *vcb;
uint64_t allocateBlocks(int blocksNeeded);  // From fsInit, reusing instead of writing our own allocator


// fs_mkdir 

int fs_mkdir(const char *pathname, mode_t mode) {
    if (pathname == NULL || strlen(pathname) == 0 || strlen(pathname) >= 32) {
        printf("[mkdir] Invalid directory name.\n");
        return -1;
    }

    // Copy pathname as parsepath modifies the input string
    char *pathCopy = strdup(pathname);
    if (!pathCopy) {
        printf("[mkdir] Memory allocation error.\n");
        return -1;
    }

    parsepathInfo ppI;
    int parseResult = parsepath(pathCopy, &ppI);
    free(pathCopy);

    if (parseResult != 0) {
        printf("[mkdir] Path parsing failed: %d\n", parseResult);
        return -1;
    }

    // Check if the directory already exists
    if (ppI.index != -1) {
        printf("[mkdir] Directory '%s' already exists.\n", ppI.lastElement);
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) {
            free(ppI.parent);
        }
        return -1;
    }

    // Ensure parent is a directory
    if (!isDEaDir(ppI.parent)) {
        printf("[mkdir] Parent is not a directory.\n");
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) {
            free(ppI.parent);
        }
        return -1;
    }

    // Parent's own directory entry (the '.' entry)
    DirectoryEntry *parentEntry = &ppI.parent[0];

    // Create the new directory using createDir
    DirectoryEntry *newDir = createDir(50, parentEntry);
    if (!newDir) {
        printf("[mkdir] Failed to create directory.\n");
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) {
            free(ppI.parent);
        }
        return -1;
    }

    // Find a free slot in the parent directory entries
    int freeSlot = -1;
    int entryCount = parentEntry->size / sizeof(DirectoryEntry);
    for (int i = 0; i < entryCount; i++) {
        if (ppI.parent[i].name[0] == '\0') {
            freeSlot = i;
            break;
        }
    }

    if (freeSlot == -1) {
        printf("[mkdir] Parent directory is full.\n");
        free(newDir);
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) {
            free(ppI.parent);
        }
        return -1;
    }

    // Populate the new directory entry in the parent
    strncpy(ppI.parent[freeSlot].name, ppI.lastElement, 31);
    ppI.parent[freeSlot].name[31] = '\0';
    ppI.parent[freeSlot].isDir = 1;
    ppI.parent[freeSlot].startBlock = newDir[0].startBlock;
    ppI.parent[freeSlot].size = newDir[0].size;
    time_t now = time(NULL);
    ppI.parent[freeSlot].created = now;
    ppI.parent[freeSlot].modified = now;
    ppI.parent[freeSlot].accessed = now;

    // Write updated parent directory back to disk
    int numBlocks = (parentEntry->size + vcb->blockSize - 1) / vcb->blockSize;
    if (LBAwrite(ppI.parent, numBlocks, parentEntry->startBlock) != numBlocks) {
        printf("[mkdir] Failed to update parent directory.\n");
        free(newDir);
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) {
            free(ppI.parent);
        }
        return -1;
    }

    // Cleanup
    free(newDir);
    if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) {
        free(ppI.parent);
    }

    printf("[mkdir] Directory '%s' created successfully at block %lu.\n", ppI.lastElement, newDir[0].startBlock);
    return 0;
}
// ------------------------------
// fs_getcwd – just returns "/" for now since we don't track actual path
// ------------------------------
char *fs_getcwd(char *pathname, size_t size) {
    if (!vcb || vcb->rootDir == 0) {
        printf("[getcwd] Error: VCB not initialized.\n");
        return NULL;
    }

    // Since we’re always in root, just return slash. Later can add path building
    strncpy(pathname, "/", size);
    printf("[getcwd] Returning current working directory: %s\n", pathname);
    return pathname;
}
