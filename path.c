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

int parsePath(const char* pathName, PPRETDATA *ppinfo){
    if(pathName == NULL || ppinfo == NULL) {
        fprintf(stderr, "invalid pointers");
        return -1;
    }
    struct DE* currDirectory;
    if(pathName[0] == '/'){
        currDirectory = loadDir(root, 0);
    }
    else {
        currDirectory = loadDir(cwd, 0);
    }
    char* savePtr = NULL;
    char* path = strdup(pathName);
    char* currToken = strtok_r(path, "/", &savePtr);
    if( currToken == NULL ) {
        if(pathName[0] == '/') {
            memcpy(ppinfo->parent, currDirectory, DE_SIZE);
            ppinfo->lastElementIndex = -2;
            ppinfo->lastElementName = NULL;
            free(path);
            free(currDirectory);
            return 0;
        }
        else {
            free(path);
            free(currDirectory);
            fprintf(stderr, "invalid path\n");
            return -1;
        }
    }
    struct DE* prevDirectory = malloc(DE_SIZE);
    memcpy(prevDirectory, currDirectory, DE_SIZE);
    int index = findInDir(prevDirectory, currToken);
    if(index != -1 && prevDirectory[index].isDirectory) {
        currDirectory = loadDir(prevDirectory, index);
    }
    char* prevToken = currToken;
    while( (currToken = strtok_r(NULL, "/", &savePtr)) != NULL ) {
        memcpy(prevDirectory, currDirectory, DE_SIZE);
        index = findInDir(prevDirectory, currToken);
        if( index == -1 ) {
            prevToken = currToken;
            currToken = strtok_r(NULL, "/", &savePtr);
            if( currToken == NULL ) {
                memcpy(ppinfo->parent, prevDirectory, DE_SIZE);
                ppinfo->lastElementIndex = -1;
                ppinfo->lastElementName = prevToken;
                return 0;
            }
            else {
                fprintf(stderr, "invalid path\n");
                return -1;
            }
        }
        else if(prevDirectory[index].isDirectory) {
            currDirectory = loadDir(prevDirectory, index);
        }
        else {
            prevToken = currToken;
            currToken = strtok_r(NULL, "/", &savePtr);
            if( currToken == NULL ) {
                break;
            }
            else {
                fprintf(stderr, "invalid path\n");
                return -1;
            }
        }
    }
    memcpy(ppinfo->parent, prevDirectory, DE_SIZE);
    ppinfo->lastElementName = prevToken;
    ppinfo->lastElementIndex = index;
    if( currDirectory != cwd && currDirectory != root ) {
        free(currDirectory);
    }
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
