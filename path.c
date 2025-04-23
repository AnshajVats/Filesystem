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
#include <b_io.c>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include "path.h"
#include "mfs.h"
#include "fsLow.h"

DirectoryEntry * alrLoadedRoot = NULL; // is a global
DirectoryEntry * alrLoadedcwd = NULL;     // is a global

// checking if given directory entry is a directory
int isDirEaDir(DirectoryEntry *de){
    if(de == NULL){
        return 0;
    }
    return de->isDir;
}

// function needs to be implemented
int FindInDirectory(DirectoryEntry *de, const char * name){
    if(!isDirEaDir(de)){
        return -1;
    }

    DirectoryEntry * entry = (DirectoryEntry *)de->name;

    for (int i = 0; i < 32; i++) {
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
DirectoryEntry * LoadDirectory(DirectoryEntry * cwd ){
    if (cwd == NULL || !cwd->isDir) {
        return NULL;
    }
    //from what we accept, we extract the block number for that Directory on disk
    //cwd->startBlock;

    
    //int lbaCount = cwd->size/B_CHUNK_SIZE; 
    int lbaCount = (cwd->size + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE;
   // Space is allocated for the directory block
    DirectoryEntry *dir = malloc(cwd->size);
    if (!dir) {
        perror("Failed to allocate memory for directory block");
        return NULL;
    }
   
    //LBAread (dir, lbaCount, cwd->startBlock);
    // That block is loaded from the disk 
    if (LBAread(dir, lbaCount, cwd->startBlock) != lbaCount) {
        perror("Failed to read the directory block from disk");
        free(dir);
        return NULL;
    }
    //from that we get DEarray and we return that directory entry array
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
            if(!isDirEaDir(&(parent[index]))){
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