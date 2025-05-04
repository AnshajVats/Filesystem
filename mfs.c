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

//Helper code 

// We added this because repeating malloc checks everywhere is messy.
// If we run out of memory, it's better to crash loudly than behave weirdly.
void* safe_malloc(size_t bytes) {
    void* mem = malloc(bytes);
    if (!mem) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);  // I’d rather catch this here than hunt down random bugs later.
    }
    return mem;
}

// This helper saves us from repeating the same path parsing logic over and over.
// It gives us both the parent directory and last element (like the new folder name) in one step.
PPRETDATA* parse_path(const char* path) {
    PPRETDATA* info = safe_malloc(sizeof(PPRETDATA));
    info->parent = safe_malloc(DE_SIZE);

    // If parsing fails, we don’t want to leave behind garbage in memory
    if (parsePath(path, info) == -1) {
        free(info->parent);
        free(info);
        return NULL;
    }
    return info;
}

// We use this because directories don’t have unlimited space.
// This checks if we can insert a new entry. Also, if we’re overwriting something,
// we make sure to release its space first (to avoid leaks or dangling data).
int find_and_validate_slot(DE* dir, const char* name, int mustBeEmpty) {
    int idx = find_vacant_space(dir, (char*)name);
    if (idx == -1) {
        fprintf(stderr, "No space left in directory.\n");
        return -1;
    }

    // Sometimes we’re reusing an old slot — if so, clean up whatever was there
    if (!mustBeEmpty) {
        DE* existing = &dir[idx];
        if (existing->location != -2 && returnFreeBlocks(existing->location) == -1) {
            return -1;
        }
    }

    return idx;
}

// Instead of manually assigning timestamps and fields every time we create a folder or file,
// we wrote this. Keeps things consistent and avoids typos (like forgetting null terminators).
int initialize_directory_entry(DE* entry, const char* name, int isFolder, int startBlock) {
    memset(entry, 0, sizeof(DE));  // Just to be safe — avoid leftover junk
    entry->location = startBlock;
    entry->isDirectory = isFolder;
    strncpy(entry->name, name, DE_NAME_SIZE);

    time_t now = time(NULL);
    entry->dateCreated = now;
    entry->dateModified = now;
    entry->dateLastAccessed = now;

    return 0;
}

// We made this because writing a directory to disk happens a lot — after mkdir, rmdir, rename, etc.
// No point copying this logic into every function. It's just cleaner this way.
int write_parent_directory(DE* parentDir) {
    int blocksToWrite = calculateFormula(parentDir->size, MINBLOCKSIZE);
    return fileWrite(parentDir, blocksToWrite, parentDir->location);
}


/*            mkdir Function          */


// fs_mkdir – Creates a new folder at a given path
// Steps:
// 1. Break the path down
// 2. Find a spot in the parent folder
// 3. Create the folder's content on disk
// 4. Add the folder to the parent
// 5. Save the changes
int fs_mkdir(const char *targetPath, mode_t mode) {
    // Step 1: Try to parse the provided path
    PPRETDATA* pathDetails = parse_path(targetPath);
    if (!pathDetails) return -1;

    // Step 2: Find a slot for the new folder inside the parent
    int slotIndex = find_and_validate_slot(pathDetails->parent, pathDetails->lastElementName, 1);
    if (slotIndex == -1) goto fail_cleanup;

    // Step 3: Actually create the directory on disk and get its block location
    int newDirBlock = createDirectory(DEFAULTDIRSIZE, pathDetails->parent);
    if (newDirBlock == -1) goto fail_cleanup;

    // Step 4: Set up the entry for the new folder
    DE newFolderEntry;
    initialize_directory_entry(&newFolderEntry, pathDetails->lastElementName, 1, newDirBlock);

    // Step 5: Add it to the parent directory and write it back
    pathDetails->parent[slotIndex] = newFolderEntry;
    int writeResult = write_parent_directory(pathDetails->parent);

    // Clean up and done
    free(pathDetails->parent);
    free(pathDetails);
    return writeResult;

fail_cleanup:
    // If anything above failed, make sure we don't leave stuff in memory
    if (pathDetails) {
        free(pathDetails->parent);
        free(pathDetails);
    }
    return -1;
}

