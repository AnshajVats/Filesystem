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


char* getLastElement(const char* path) {
    if (strlen(path) == 1 && path[0] == '/') {
        return strdup("."); // Return allocated string
    }

    const char* lastSlash = strrchr(path, '/');
    if (!lastSlash) return strdup(path); // No slashes found

    // Handle trailing slash
    if (*(lastSlash + 1) == '\0') {
        char* p = strdup(path);
        p[strlen(p)-1] = '\0'; // Remove trailing slash
        const char* newSlash = strrchr(p, '/');
        char* result = newSlash ? strdup(newSlash + 1) : strdup(p);
        free(p);
        return result;
    }

    return strdup(lastSlash + 1); // Return allocated substring
}


//----------------------------------------------------------------
// fs_mdLs
// This function lists the contents of a directory.
//-------------------------------------------------------------------
fdDir * fs_opendir(const char *pathname) {
    PPRETDATA *ppinfo = malloc(sizeof(PPRETDATA));
    ppinfo->parent = malloc(DE_SIZE); // TODO: why not malloc in pp?
    int res = parsePath(pathname, ppinfo);

    if (res == -1) {
        return NULL;
    }

    const char* path = pathname;
    char* lastElementName = getLastElement(path);
    int index = findInDir(ppinfo->parent, lastElementName);

    if (index == -1) {
        fprintf(stderr, "fs_opendir: %s not found\n", lastElementName);
        return NULL;
    }

    struct DE entry = ppinfo->parent[index];
    if (!entry.isDirectory) {
        fprintf(stderr, "%s is not a directory\n", pathname);
        return NULL;
    }

    fdDir *fd = malloc(sizeof(fdDir));

    fd->d_reclen = calculateFormula(entry.size, vcb->blockSize);
    fd->dirEntryPosition = index;
    fd->dirEntryLocation = entry.location;
    fd->index = 0;

    // TODO: why tf was this not typedef?? pick one!!11!
    fd->di = malloc(sizeof(struct fs_diriteminfo));
    fd->di->d_reclen = calculateFormula(entry.size, vcb->blockSize);
    fd->di->fileType = entry.isDirectory;
    strcpy(fd->di->d_name, lastElementName);

    free(ppinfo->parent);
    free(ppinfo);
    return fd;
}


//----------------------------------------------------------------
// fs_readdir
// This function reads the next entry in a directory.
//-------------------------------------------------------------------

struct fs_diriteminfo *fs_readdir(fdDir *dirp){
    int num_blocks = calculateFormula(sizeof(DE) * DECOUNT, vcb->blockSize);
    struct DE* entries = malloc(num_blocks * vcb->blockSize);
    int res = fileRead(entries, num_blocks, dirp->dirEntryLocation);
    struct DE entry = entries[dirp->index];

    if (res == -1) {
        fprintf(stderr, "Could not load entry\n");
        free(entries);
        return NULL;
    }

    // Skip empty entries
    while (dirp->index < DECOUNT-1 && entry.location == -2l) {
        entry = entries[++dirp->index];
    }

    if (dirp->index == DECOUNT-1) {
        free(entries);
        return NULL;
    }

    dirp->di->d_reclen = dirp->d_reclen;
    dirp->di->fileType = entry.isDirectory;
    strcpy(dirp->di->d_name, entry.name);
    dirp->index++;

    free(entries);
    return dirp->di;
}

//--------------------------------------------------------------
// fs_stat
// This function retrieves information about a file or directory.
//--------------------------------------------------------------

int fs_stat(const char *pathname, struct fs_stat *buf) {
    PPRETDATA *ppinfo = malloc(sizeof(PPRETDATA));
    ppinfo->parent = malloc(DE_SIZE); // TODO: why not malloc in pp?
    int res = parsePath(pathname, ppinfo);

    if (res == -1) {
        return -1;
    }

    char* lastElementName = getLastElement(pathname);
    int index = findInDir(ppinfo->parent, lastElementName);
    if (index == -1) {
        return -1;
    }

    struct DE entry = ppinfo->parent[index];
    buf->st_size = entry.size;
    buf->st_blksize = vcb->blockSize;
    buf->st_blocks = calculateFormula(entry.size, vcb->blockSize);
    buf->st_accesstime = entry.dateLastAccessed;
    buf->st_modtime = entry.dateModified;
    buf->st_createtime = entry.dateLastAccessed;

    return index;
}


//--------------------------------------------------------------
// fs_closedir
// This function closes a directory stream.
//--------------------------------------------------------------

int fs_closedir(fdDir *dirp) {

    if (dirp == NULL) {
        fprintf(stderr, "Cannot close directory, is null\n");
        return 0;
    }

    free(dirp);
    return 1;
}


//----------------------------------------------------------------
// fs_isFile
// This function checks if a given path is a file.
//-------------------------------------------------------------------
//return 1 if directory, 0 otherwise
int fs_isDir(char * pathname){
    PPRETDATA *ppinfo = malloc(sizeof(PPRETDATA));
    ppinfo->parent = malloc(DE_SIZE);
    int res = parsePath(pathname, ppinfo);

    if (res == -1 || ppinfo->lastElementIndex < 0) {
        free(ppinfo->parent);
        free(ppinfo);
        return 0;
    }

    int returnStatement = ppinfo->parent[ppinfo->lastElementIndex].isDirectory;
    free(ppinfo->parent);
    free(ppinfo);
    return returnStatement;
}

//return 1 if file, 0 otherwise
int fs_isFile(char * filename){
	int index = findInDir(cwd, filename);
    if( index == -1 ) {
        return -1;
    }
	return !cwd[index].isDirectory;
}