/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name:: Wilmaire & Karina Alvarado Mendoza
* Student IDs::
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
* Project:: Basic File System
*
* File:: path.c
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
#include "path.h"
#include "mfs.h"
#include "fsLow.h"
#include "fsInit.h"
#include "b_io.h"

#define B_CHUNK_SIZE 512 // block size in bytes

// global pointers


// helper function
// checking if given directory entry is a directory

int findInDir(DE* searchDirectory, char* name){
    int res = -1;
    for( int i = 0; i < DECOUNT; i++) {
        if( searchDirectory[i].location != -2l && strcmp(searchDirectory[i].name, name ) == 0) {
            res = i;
        }
    }
    return res;
}


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

void freeDirectory(DE *dir) {
    if (dir != cwd && dir != root) {
        free(dir);
    }
}

struct DE* getStartingDirectory(const char* pathName) {
    return (pathName[0] == '/') ? loadDir(root, 0) : loadDir(cwd, 0);
}

int handleRootCase(const char* pathName, DE* currDir, PPRETDATA* ppinfo) {
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
    
    freeDirectory(*currDir);
    *currDir = newDir;
    return 1;
}

void processLastComponent(DE* currDir, char* token, PPRETDATA* ppinfo) {
    int index = findInDir(currDir, token);
    memcpy(ppinfo->parent, currDir, DE_SIZE);
    ppinfo->lastElementIndex = index;
    ppinfo->lastElementName = strdup(token); // Remember to free this later
}

int parsePath(const char* pathName, PPRETDATA* ppinfo) {
    if (!pathName || !ppinfo) {
        fprintf(stderr, "Invalid pointers\n");
        return -1;
    }

    DE* currDir = getStartingDirectory(pathName);
    if (!currDir) return -1;

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
PPRETDATA* parseFilePath(char *filename, DE **fileInfo) {
    PPRETDATA *ppinfo = malloc(sizeof(PPRETDATA));
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
int handleReadMode(b_io_fd fd, PPRETDATA *ppinfo) {
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
int handleWriteMode(b_io_fd fd, PPRETDATA *ppinfo, int flags) {
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
int handleCreateMode(b_io_fd fd, PPRETDATA *ppinfo) {
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
void cleanupResources(PPRETDATA *ppinfo, DE *fileInfo, b_fcb *fcb) {
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