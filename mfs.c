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


//----------------------------------------------------------------
// fsmkdir
// This function creates a new directory at the specified path.
//-------------------------------------------------------------------
int fs_mkdir (const char *pathname, mode_t mode){
	int ret;				// Used for error handling
	int emptyIndex;				// Track first unused entry in parent
	char * directoryName;			// Name of new directory
	PPRETDATA *parsepathinfo;	// Structure returned from parse path
	DE * parent;			// Track parent directory entry
	DE * newDirectory;		// Used for new directory entry

	parsepathinfo = malloc(sizeof(PPRETDATA));
	parsepathinfo->parent = malloc(DE_SIZE);
	newDirectory = malloc(MINBLOCKSIZE);
	if ( !parsepathinfo || !newDirectory ){
		perror("malloc");
		return -1;
	}

	if( parsePath(pathname, parsepathinfo) == -1 ) {
        fprintf(stderr, "invalid path\n");
        return -1;
    }
	// Read all relevant data from parsepath needed for directory creation
	parent = parsepathinfo -> parent;
	directoryName = parsepathinfo->lastElementName;
	emptyIndex = find_vacant_space ( parent , directoryName);
	if ( emptyIndex == -1){
		fprintf(stderr, "error in Find Vacant Space\n");
		return -1;
	}

	// Create and link new directory to an empty position in parent array
	ret = createDirectory(DEFAULTDIRSIZE, parent);
	if ( ret == -1 ){
		fprintf(stderr, "error in Create Directory\n");
		return -1;
	}
	memset(newDirectory, 0, MINBLOCKSIZE);
	ret = fileRead(newDirectory, 1, ret);
	if ( ret == -1){
		fprintf(stderr, "error in File Read\n");
		return -1;
	}
	parent[emptyIndex] = newDirectory[0];
	strncpy(parent[emptyIndex].name, directoryName, DE_NAME_SIZE);

	// Write changes back to parent directory to complete linking
    int size = calculateFormula(parent->size, MINBLOCKSIZE);
	fileWrite(parent, size, parent->location);

	return 0;
}


//----------------------------------------------------------------
// fs_setcwd
// This function sets the current working directory to the specified path.
//-------------------------------------------------------------------
char* cleanPath(char* pathname) {
    char** pathTable = malloc(sizeof(char*)*strlen(pathname)/2);
    char* savePtr = NULL;
    char* token = strtok_r(pathname, "/", &savePtr);
    int size = 0;
    while( token != NULL ) {
        pathTable[size] = strdup(token);
        token = strtok_r(NULL, "/", &savePtr);
        size++;
    }
    int* indices = malloc(sizeof(int) * size);
    int index = 0;
    int i = 0;
    while( i < size ) {
        char* token = pathTable[i];
        if( strcmp(token, ".") == 0 );
        else if( strcmp(token, "..") == 0 && index > 0) {
            index--;
        }
        else if( strcmp(token, "..") == 0 && index == 0  );
        else {
            indices[index] = i;
            index++;
        }
        i++;
    }
    char* res = malloc( strlen(pathname));
    strcpy(res, "/");
    for( int i = 0; i < index; i++ ) {
        strcat(res, pathTable[indices[i]]);
        strcat(res, "/");
    }
    return res;
}

char * fs_getcwd(char *pathname, size_t size){
    strncpy(pathname, cwdPathName, size);
    return cwdPathName;
}


int fs_setcwd(char *pathname){
    PPRETDATA *ppinfo = malloc( sizeof(PPRETDATA));
    ppinfo->parent = malloc( DE_SIZE );
    int res = parsePath(pathname, ppinfo);
    if( ppinfo->lastElementIndex == -2 ) {
        cwd = loadDir(root, 0);
        strcpy(cwdPathName, "/");
        free(ppinfo->parent);
        free(ppinfo);
        return 0;
    }
    if( res == -1 || ppinfo->lastElementIndex == -1){
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }
    struct DE* dir = malloc(512 * 7 );
    dir = loadDir(ppinfo->parent, ppinfo->lastElementIndex);
    if( dir->isDirectory != 1 ) {
        free(dir);
        return -1;
    }
    memcpy(cwd, dir, DE_SIZE);
    free(dir);
    if( pathname[0] == '/' ) {
        cwdPathName = strdup(pathname);
    }
    else {
        strcat(cwdPathName, pathname);
    }
    cwdPathName = cleanPath(cwdPathName);
    int size = calculateFormula(cwd->size, MINBLOCKSIZE);
    fileWrite(cwd, size, cwd->location);
    return 0;
}

int calculateFormula(int i, int j){
    return (i+j-1)/j;
}



