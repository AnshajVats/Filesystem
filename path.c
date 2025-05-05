/**************************************************************
* Class::  CSC-415-02 Spring 2025
* Name:: Wilmaire & Karina Alvarado Mendoza, Anshak and Ayesha
* Student IDs:: 923656653, 923760991
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
* Project:: Basic File System
*
* File:: path.c
*
* Description:: This file implements core path-handling logic for the file system.
*    It includes functionality for breaking down file paths into components,
*    resolving relative and absolute paths, and navigating the directory structure.
*    It supports parsing operations used by commands like mkdir, cd, ls, mv, and rm,
*    enabling them to correctly identify parent directories and final path elements.
*
**************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include "path.h"
#include "mfs.h"
#include "fsLow.h"
#include "fsInit.h"
#include "b_io.h"

#define B_CHUNK_SIZE 512 // block size in bytes
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// global pointers


// helper function
// checking if given directory entry is a directory

// finds directory for entry with specified name
// returns index of its matching
int findInDir(DE* searchDirectory, char* name){
    int res = -1;
    for( int i = 0; i < DECOUNT; i++) {
        if( searchDirectory[i].location != -2l && strcmp(searchDirectory[i].name, name ) == 0) {
            res = i;
        }
    }
    return res;
}

// loads directory from disk into memory
DE* loadDir(DE* searchDirectory, int index) {
    int size = calculateFormula(DE_SIZE, MINBLOCKSIZE);
    int loc = searchDirectory[index].location;
    struct DE* directories = (struct DE*)malloc(DE_SIZE);
    int res = fileRead(directories, size, loc);
    if( res == -1 ) {
        free(directories);
        return NULL;
    }
    return directories;
}

// frees memory used directory
void freeDirectory(DE *dir) {
    if (dir != cwd && dir != root) {
        free(dir);
    }
}

// determines staring directory
struct DE* getStartingDirectory(const char* pathName) {
    return (pathName[0] == '/') ? loadDir(root, 0) : loadDir(cwd, 0);
}

// handles special case when path refers to root
int handleRootCase(const char* pathName, DE* currDir, PathParseResult* ppinfo) {
    if (pathName[0] == '/') {
        memcpy(ppinfo->parent, currDir, DE_SIZE);
        ppinfo->lastElementIndex = -2;
        ppinfo->lastElementName = NULL;
        return 0;
    }
    fprintf(stderr, "invalid path\n");
    return -1;
}

int processDirectoryComponent(DE** currDir, char* token) {
    int index = findInDir(*currDir, token);
    if (index == -1 || !(*currDir)[index].isDirectory) {
        fprintf(stderr, "Component '%s' not found or not a directory\n", token);
        return 0;
    }
    
    DE* newDir = loadDir(*currDir, index);
    if (!newDir) return 0;
    
    freeDirectory(*currDir);    // frees old directory if needed
    *currDir = newDir;          // updates current directory pointer
    return 1;
}

// saves info about last elemnt
void processLastComponent(DE* currDir, char* token, PathParseResult* ppinfo) {
    int index = findInDir(currDir, token);
    memcpy(ppinfo->parent, currDir, DE_SIZE);
    ppinfo->lastElementIndex = index;
    ppinfo->lastElementName = strdup(token); // Remember to free this later
}

// parses given path
int parsePath(const char* pathName, PathParseResult* ppinfo) {
    if (!pathName || !ppinfo) {
        fprintf(stderr, "Invalid pointers\n");
        return -1;
    }

    // determines starting directory (root or cwd)
    DE* currDir = getStartingDirectory(pathName);
    if (!currDir) return -1;

    // modifiablle copy of path
    char* pathCopy = strdup(pathName);
    if (!pathCopy) {
        freeDirectory(currDir);
        return -1;
    }

    char *savePtr, *currToken = strtok_r(pathCopy, "/", &savePtr);
    if (!currToken) {
        int result = handleRootCase(pathName, currDir, ppinfo);
        free(pathCopy);
        freeDirectory(currDir);
        return result;
    }

    // processing each component
    while (1) {
        char *nextToken = strtok_r(NULL, "/", &savePtr);
        
        if (!nextToken) { // Last component
            processLastComponent(currDir, currToken, ppinfo);
            break;
        }
        
        if (!processDirectoryComponent(&currDir, currToken)) {
            free(pathCopy);
            freeDirectory(currDir);
            return -1;
        }
        currToken = nextToken;
    }

    free(pathCopy);
    freeDirectory(currDir);
    return 0;
}

int find_vacant_space(DE * directory, char * fileName){
    int firstVacant = -1;
    
    for (int i = 0; i < (directory->size)/sizeof(struct DE); i++) {
        // Keep track of the first vacant slot we find
        if (firstVacant == -1 && (directory + i)->location == -2) {
            firstVacant = i;
        }
        
        // Only check for duplicates in non-vacant entries
        if ((directory + i)->location != -2 && 
            strcmp(fileName, (directory + i)->name) == 0) {
            fprintf(stderr, "Duplicate found");
            return -1;
        }
    }
    
    if (firstVacant == -1) {
        fprintf(stderr, "Directory is full");
        return -1;
    }
    
    return firstVacant;
}


    // Initializes system and allocates FCB resources
b_io_fd initializeFCB() {
    if (startup == 0) b_init();
    b_io_fd fd = b_getFCB();
    if (fd == -2) {
        fprintf(stderr, "Error getting FCB\n");
        return -1;
    }

    b_fcb *fcb = &fcbArray[fd];
    fcb->fileInfo = malloc(sizeof(DE));
    fcb->buf = malloc(B_CHUNK_SIZE);
    if (!fcb->fileInfo || !fcb->buf) {
        free(fcb->fileInfo);
        free(fcb->buf);
        return -1;
    }
    return fd;
}

// Parses the file path and retrieves directory information
PathParseResult* parseFilePath(char *filename, DE **fileInfo) {
    PathParseResult *ppinfo = malloc(sizeof(PathParseResult));
    if (!ppinfo) return NULL;
    ppinfo->parent = malloc(DE_SIZE);
    *fileInfo = malloc(MINBLOCKSIZE);
    if (!ppinfo->parent || !*fileInfo) {
        free(ppinfo);
        free(*fileInfo);
        return NULL;
    }
    memset(*fileInfo, 0, MINBLOCKSIZE);

    if (parsePath(filename, ppinfo) == -1) {
        free(ppinfo->parent);
        free(ppinfo);
        free(*fileInfo);
        return NULL;
    }
    return ppinfo;
}

// Handles opening a file for reading
int handleReadMode(b_io_fd fd, PathParseResult *ppinfo) {
    b_fcb *fcb = &fcbArray[fd];
    DE *file = &ppinfo->parent[ppinfo->lastElementIndex];
    memcpy(fcb->fileInfo, file, sizeof(DE));
    fcb->remainingBytes = file->size;
    fcb->index = 0;
    fcb->fileIndex = ppinfo->lastElementIndex;
    fcb->numBlocks = 0;
    return 1;
}

// Handles opening a file for writing (existing file)
int handleWriteMode(b_io_fd fd, PathParseResult *ppinfo, int flags) {
    b_fcb *fcb = &fcbArray[fd];
    DE *file = &ppinfo->parent[ppinfo->lastElementIndex];
    memcpy(fcb->fileInfo, file, sizeof(DE));
    fcb->remainingBytes = file->size;
    fcb->currentBlock = file->location;
    fcb->fileIndex = ppinfo->lastElementIndex;
    fcb->numBlocks = calculateFormula(file->size, MINBLOCKSIZE);

    if (flags & O_TRUNC) {
        fcb->currentBlock = fileSeek(fcb->currentBlock, fcb->numBlocks);
        fcb->index = file->size % B_CHUNK_SIZE;
        fileRead(fcb->buf, 1, fcb->currentBlock);
    }
    return 1;
}

// Handles creating a new file
int handleCreateMode(b_io_fd fd, PathParseResult *ppinfo) {
    b_fcb *fcb = &fcbArray[fd];
    time_t currTime = time(NULL);
    fcb->fileInfo->name[DE_NAME_SIZE - 1] = '\0';
    strncpy(fcb->fileInfo->name, ppinfo->lastElementName, DE_NAME_SIZE - 1);
    fcb->fileInfo->size = 0;
    fcb->fileInfo->location = -1; // To be allocated later
    fcb->fileInfo->dateCreated = currTime;
    fcb->fileInfo->dateModified = currTime;
    fcb->fileInfo->dateLastAccessed = currTime;
    fcb->fileInfo->isDirectory = 0;
    fcb->remainingBytes = 0;
    fcb->index = 0;
    fcb->numBlocks = 0;
    fcb->fileIndex = find_vacant_space(ppinfo->parent, fcb->fileInfo->name);
    return fcb->fileIndex >= 0 ? 1 : 0;
}

// Sets common fields for the FCB
void setupCommonFCBFields(b_io_fd fd, DE *parent, int flags) {
    b_fcb *fcb = &fcbArray[fd];
    fcb->parent = parent;
    fcb->buflen = B_CHUNK_SIZE;
    fcb->activeFlags = flags;
}

// Writes the updated parent directory back to disk
void updateParentDirectory(b_io_fd fd) {
    b_fcb *fcb = &fcbArray[fd];
    int parentBlocks = calculateFormula(DE_SIZE, MINBLOCKSIZE);
    fcb->parent[fcb->fileIndex] = *fcb->fileInfo;
    fileWrite(fcb->parent, parentBlocks, fcb->parent[0].location);
}

// Frees allocated resources during error handling
void cleanupResources(PathParseResult *ppinfo, DE *fileInfo, b_fcb *fcb) {
    if (ppinfo) {
        free(ppinfo->parent);
        free(ppinfo);
    }
    free(fileInfo);
    if (fcb) {
        free(fcb->fileInfo);
        free(fcb->buf);
        free(fcb->parent);
    }
}

// validates whether write operation is allowed
int validate_write(b_io_fd fd, int count) {
    if (startup == 0) b_init();
    
    if (fd < 0 || fd >= MAXFCBS) {
        fprintf(stderr, "Invalid file descriptor: %d\n", fd);
        return -1;
    }
    
    b_fcb *fcb = &fcbArray[fd];
    if (!fcb->fileInfo || count < 0 || (fcb->activeFlags & O_RDONLY)) {
        fprintf(stderr, "Invalid write parameters\n");
        return -1;
    }
    
    return 0;
}

// making sure there is enough space allocated
int ensure_space(b_fcb *fcb, int required_bytes) {
    int current_space = fcb->numBlocks * MINBLOCKSIZE;
    int needed_blocks = (required_bytes - (current_space - fcb->fileInfo->size) 
                       + MINBLOCKSIZE - 1) / MINBLOCKSIZE;
    
    if (needed_blocks > 0) {
        int new_blocks = allocateblock(needed_blocks);
        if (new_blocks == -1) return -1;

        if (fcb->fileInfo->location == -1) {
            fcb->fileInfo->location = new_blocks;
            fcb->currentBlock = new_blocks;
        } else {
            int last_block = fileSeek(fcb->fileInfo->location, fcb->numBlocks - 1);
            freeSpaceMap[last_block] = new_blocks;
        }
        fcb->numBlocks += needed_blocks;
    }
    return 0;
}

// writes data to file 
int perform_write(b_fcb *fcb, char *data, int length) {
    int total = 0;
    
    int buff_space = B_CHUNK_SIZE - fcb->index;
    if (buff_space > 0) {
        int chunk = MIN(buff_space, length);
        memcpy(fcb->buf + fcb->index, data, chunk);
        fcb->index += chunk;
        total += chunk;
        length -= chunk;
        data += chunk;
    }

    if (fcb->index == B_CHUNK_SIZE) {
        fileWrite(fcb->buf, 1, fcb->currentBlock);
        fcb->currentBlock = fileSeek(fcb->currentBlock, 1);
        fcb->index = 0;
    }

    if (length >= B_CHUNK_SIZE) {
        int blocks = length / B_CHUNK_SIZE;
        int written = fileWrite(data, blocks, fcb->currentBlock);
        fcb->currentBlock = fileSeek(fcb->currentBlock, written);
        int bytes = written * B_CHUNK_SIZE;
        total += bytes;
        length -= bytes;
        data += bytes;
    }

    // Store remaining in buffer
    if (length > 0) {
        memcpy(fcb->buf, data, length);
        fcb->index = length;
        total += length;
    }

    return total;
}

int validate_read(b_io_fd fd, int count, b_fcb **fcb_ptr) {
    if (startup == 0) b_init();
    
    if (fd < 0 || fd >= MAXFCBS) {
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }
    
    *fcb_ptr = &fcbArray[fd];
    
    if ((*fcb_ptr)->fileInfo == NULL || count < 0 || 
        ((*fcb_ptr)->activeFlags & O_WRONLY)) {
        fprintf(stderr, "Invalid read parameters\n");
        return -1;
    }
    
    return 0;
}

void calculate_segments(b_fcb *fcb, int count, 
                              int *part1, int *part2, int *part3) {
    // Initialize buffer if first read
    if (fcb->remainingBytes == fcb->fileInfo->size) {
        fcb->index = 0;
        fcb->buflen = fileRead(fcb->buf, 1, fcb->currentBlock) * B_CHUNK_SIZE;
    }
    
    int bytes_available = fcb->buflen - fcb->index;
    int readable = (count > fcb->remainingBytes) ? fcb->remainingBytes : count;
    
    *part1 = (bytes_available > readable) ? readable : bytes_available;
    *part2 = (readable - *part1) / B_CHUNK_SIZE * B_CHUNK_SIZE;
    *part3 = readable - *part1 - *part2;
}
