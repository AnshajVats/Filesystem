#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include "mfs.h"
#include "fsLow.h"

#define INVALID_BLOCK ((uint64_t)-1)  // Used to track if allocation failed
#define EOC -2  // We define end of chain for FAT-like setup, useful for debugging later

extern VCB *vcb;
uint64_t allocateBlocks(int blocksNeeded);  // From fsInit, reusing instead of writing our own allocator


// fs_mkdir 

int fs_mkdir(const char *pathname, mode_t mode) {
    // We bail early if name is missing or too long, this avoids garbage in the FS
    if (pathname == NULL || strlen(pathname) == 0 || strlen(pathname) >= 32) {
        printf("[mkdir] Invalid directory name given.\n");
        return -1;
    }

    // We calculate how many entries can fit in a block so we know loop bounds later
    int entryCount = vcb->blockSize / sizeof(DirectoryEntry);

    // This buffer will hold the root directory table; we'll add to it later
    DirectoryEntry *rootBuf = malloc(vcb->blockSize);
    if (!rootBuf) {
        printf("[mkdir] Failed to allocate memory for root.\n");
        return -1;
    }

    // Pull root directory block into memory so we can search it
    if (LBAread(rootBuf, 1, vcb->rootDir) != 1) {
        printf("[mkdir] Failed to read root directory from disk.\n");
        free(rootBuf);
        return -1;
    }

    // Check if directory already exists. It helps prevent duplicates
    for (int i = 0; i < entryCount; i++) {
        if (strncmp(rootBuf[i].name, pathname, 31) == 0) {
            printf("[mkdir] Directory '%s' already exists.\n", pathname);
            free(rootBuf);
            return -1;
        }
    }

    // Allocate fresh space for the directory contents – using calloc to zero it out
    DirectoryEntry *ndir = calloc(entryCount, sizeof(DirectoryEntry));
    if (!ndir) {
        printf("[mkdir] Could not allocate memory for new dir block.\n");
        free(rootBuf);
        return -1;
    }

    // Ask disk for a free block. If this fails, there's no room left
    uint64_t blk = allocateBlocks(1);
    if (blk == INVALID_BLOCK) {
        printf("[mkdir] No space left to allocate new block.\n");
        free(rootBuf);
        free(ndir);
        return -1;
    }

    // We'll reuse this for timestamps so all three (created, mod, access) match
    time_t now = time(NULL);

    // The "." entry just points to itself. Irequired in every directory
    strcpy(ndir[0].name, ".");
    ndir[0].isDir = 1;
    ndir[0].startBlock = blk;
    ndir[0].size = sizeof(DirectoryEntry) * 2;
    ndir[0].created = ndir[0].modified = ndir[0].accessed = now;

    // ".." should point back to parent (which is root for now)
    strcpy(ndir[1].name, "..");
    ndir[1].isDir = 1;
    ndir[1].startBlock = rootBuf[0].startBlock;
    ndir[1].created = ndir[1].modified = ndir[1].accessed = now;

    // Actually write the new directory block to disk
    if (LBAwrite(ndir, 1, blk) != 1) {
        printf("[mkdir] Failed to write new directory block.\n");
        free(rootBuf);
        free(ndir);
        return -1;
    }

    // We loop again to find a free slot in root and plug in the new folder info
    for (int i = 0; i < entryCount; i++) {
        if (rootBuf[i].name[0] == '\0') {
            strncpy(rootBuf[i].name, pathname, 31);
            rootBuf[i].isDir = 1;
            rootBuf[i].startBlock = blk;
            rootBuf[i].size = sizeof(DirectoryEntry) * 2;
            rootBuf[i].created = rootBuf[i].modified = rootBuf[i].accessed = now;
            break;
        }
    }

    // Save the updated root table back to disk now that it has the new entry
    if (LBAwrite(rootBuf, 1, vcb->rootDir) != 1) {
        printf("[mkdir] Failed to update root directory.\n");
        free(rootBuf);
        free(ndir);
        return -1;
    }

    // At this point, everything works. It prints block location for debugging
    printf("[mkdir] New directory block allocated at %lu\n", blk);
    printf("[mkdir] Directory '%s' created successfully.\n", pathname);

    free(rootBuf);
    free(ndir);
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
