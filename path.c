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

int find_vacant_space (DE * directory , char * fileName){
	for (int i = 0 ; i < (directory->size)/sizeof(struct DE) ; i++ ) {
		if ( strcmp(fileName, (directory +i)->name) == 0) {
			fprintf(stderr, "Duplicate found");
			return -1;
		}
		if ( (directory + i)->location == -2 ){
			return i;
		}
	}
	fprintf(stderr, "Directory is full");
	return -1;
}
