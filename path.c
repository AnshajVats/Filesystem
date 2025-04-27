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

#define B_CHUNK_SIZE 512 // block size in bytes

// global pointers


// helper function
// checking if given directory entry is a directory
int isDEaDir(DirectoryEntry *de){
    if(de == NULL){
        return 0;
    }
    return de->isDir;
}

// helper function
// finds given directory
int FindInDirectory(DirectoryEntry *entry, const char * name){
    if(!isDEaDir(entry)){
        return -1;
    }

    // iterates through entries to find match
    for (int i = 0; i < 32; i++) {
        
        // checking if dir is valid
        if (entry[i].name[0] == '\0') {
            break;
        }
        if(strcmp(entry[i].name, name) == 0){
            return i;
        }
    }
    return -1;
}

// accepting one directory entry/some index in array
// path.c
DirectoryEntry *LoadDirectory(DirectoryEntry *cwd) {
    if (!cwd || !isDEaDir(cwd) || cwd->size <= 0) {
        return NULL; // Validate size
    }

    // Calculate block count using directory size and block size
    int lbaCount = (cwd->size + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE;

    // Allocate block-aligned memory
    DirectoryEntry *dir = malloc(lbaCount * B_CHUNK_SIZE); // <-- Fixed
    if (!dir) return NULL;

    // Load directory blocks from disk
    if (LBAread(dir, lbaCount, cwd->startBlock) != lbaCount) {
        free(dir);
        return NULL;
    }
    return dir;
}
// needed for most functions
int parsepath(char * pathname, parsepathInfo * ppI) {

    // variable in use
    DirectoryEntry * parent;        // pointer to current directory
    DirectoryEntry * startParent;   // pinter to start of directory
    char * savePtr;                 // keep track of strtok_r position
    char * token1;                  // first pathname iterated
    char * token2;                  // for next iteration

    // check if pathname exists
    if (pathname == NULL) {
        return -1;
    }

    // checking where we are
    if(pathname[0] == '/') {
        startParent = alrLoadedRoot;
    } else{
        startParent = alrLoadedcwd;
    }

    // initialize parent as starting directory
    parent = startParent;

    // tokenize pathname
    token1 = strtok_r(pathname, "/", &savePtr);

    // checking if token1 is root
    if(token1 == NULL){
        // handles token as root
        if(pathname[0] == '/'){
            ppI->parent = parent;
            ppI->index = -2; // since root has no name and cannot be -1
            ppI->lastElement = NULL;
            return 0;
        } else{
            return -1;
        }
    }

    while (1) {
        int index = FindInDirectory(parent, token1);
        // next token for next pathname
        token2 = strtok_r(NULL, "/", &savePtr);
        
        // checking if it is last element
        if(token2 == NULL){
            ppI->parent = parent;
            ppI->index = index;
            ppI->lastElement = token1;
            return 0;
        } else{
            if(index == -1){
                return -2;
            }
            if(!isDEaDir(&(parent[index]))){
                return -1;
            }
            DirectoryEntry * tempParent = LoadDirectory(&(parent[index]));
            if(tempParent == NULL){
                return -1;
            }
            if(parent != startParent){
                free(parent);
            }
            parent = tempParent;
            token1 = token2;
        }
    }
    
    return 0;
}