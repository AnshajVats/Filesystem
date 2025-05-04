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

//-----------------------------------------------------------------
// fs_rmdir
// This function removes a directory at the specified path.
//-----------------------------------------------------------------
int isEmpty(struct DE* dir) {
    printf("DEBUG: Checking if directory is empty\n");
    for (int i = 2; i < DECOUNT; i++) {
        printf("DEBUG: Entry %d location: %ld\n", i, (long)dir[i].location);
        if (dir[i].location > 0) {
            printf("DEBUG: Directory not empty, found entry at index %d\n", i);
            return 0;
        }
    }
    printf("DEBUG: Directory is empty\n");
    return 1;
}


int fs_rmdir(const char *pathname) {
    PPRETDATA *ppinfo = malloc(sizeof(PPRETDATA));
    ppinfo->parent = malloc(DE_SIZE);
    printf("DEBUG: fs_rmdir called with pathname: %s\n", pathname);
    
    int res = parsePath(pathname, ppinfo);
    printf("DEBUG: parsePath returned %d\n", res);
    
    int index = ppinfo->lastElementIndex;
    if (res == -1 || index <= -1) {
        printf("DEBUG: Invalid path or index\n");
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }
    
    printf("DEBUG: Entry isDirectory: %d\n", ppinfo->parent[index].isDirectory);
    
    // Check if this is a file or directory
    if (!ppinfo->parent[index].isDirectory) {
        printf("DEBUG: Target is a file, handling as file deletion\n");
        
        // Free the blocks allocated to the file
        if (ppinfo->parent[index].size > 0) {
            int result = returnFreeBlocks(ppinfo->parent[index].location);
            printf("DEBUG: returnFreeBlocks returned %d\n", result);
            if (result == -1) {
                free(ppinfo->parent);
                free(ppinfo);
                return -1;
            }
        }
        
        // Mark file entry as unused
        ppinfo->parent[index].location = -2;
        printf("DEBUG: Marked file entry %d in parent directory as unused\n", index);
        
        // Write updated parent directory back to disk
        int size = calculateFormula(ppinfo->parent->size, vcb->blockSize);
        int location = ppinfo->parent->location;
        int writeResult = fileWrite(ppinfo->parent, size, location);
        printf("DEBUG: fileWrite returned %d (expected size: %d)\n", writeResult, size);
        
        free(ppinfo->parent);
        free(ppinfo);
        return 0;
    }
    
    // If we get here, it's a directory
    printf("DEBUG: Target is a directory\n");
    struct DE* currDir = loadDir(ppinfo->parent, index);
    if (!currDir) {
        printf("DEBUG: Failed to load directory\n");
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }
    
    // Check if directory is empty
    if (isEmpty(currDir) == 0) {
        printf("DEBUG: Directory is not empty\n");
        free(currDir);
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }
    
    // Free the blocks allocated to the directory
    int result = returnFreeBlocks(ppinfo->parent[index].location);
    printf("DEBUG: returnFreeBlocks returned %d\n", result);
    if (result == -1) {
        free(currDir);
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }
    
    // Mark directory entry as unused
    ppinfo->parent[index].location = -2;
    printf("DEBUG: Marked directory entry %d in parent directory as unused\n", index);
    
    // Write updated parent directory back to disk
    int size = calculateFormula(ppinfo->parent->size, vcb->blockSize);
    int location = ppinfo->parent->location;
    int writeResult = fileWrite(ppinfo->parent, size, location);
    printf("DEBUG: fileWrite returned %d (expected size: %d)\n", writeResult, size);
    
    free(currDir);
    free(ppinfo->parent);
    free(ppinfo);
    return 0;
}

//----------------------------------------------------------------
// fs_delete
// This function deletes a file at the specified path.
//-------------------------------------------------------------------
int fs_delete(char* filename) {
    PPRETDATA *ppinfo = malloc(sizeof(PPRETDATA));
    if (!ppinfo) {
        return -1;  // Memory allocation failed
    }
    
    ppinfo->parent = malloc(DE_SIZE);
    if (!ppinfo->parent) {
        free(ppinfo);
        return -1;  // Memory allocation failed
    }
    
    int res = parsePath(filename, ppinfo);
    int index = ppinfo->lastElementIndex;
    
    if (res == -1 || index == -1) {
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }
    
    // Check if it's a directory - we shouldn't delete directories with rm
    if (ppinfo->parent[index].isDirectory) {
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }
    
    // Return the blocks allocated to this file to the free space map
    if (ppinfo->parent[index].size > 0 && ppinfo->parent[index].location >= 0) {
        if (returnFreeBlocks(ppinfo->parent[index].location) == -1) {
            free(ppinfo->parent);
            free(ppinfo);
            return -1;
        }
    }
    
    // Mark the directory entry as unused
    ppinfo->parent[index].location = -2;
    
    // Add debugging to verify the entry was marked as unused
    printf("DEBUG: Marked entry %d in parent directory as unused (location=%ld)\n", 
           index, (long)ppinfo->parent[index].location);
    
    // Write the updated parent directory back to disk
    int dirSize = calculateFormula(ppinfo->parent[0].size, MINBLOCKSIZE);
    int writeResult = fileWrite(ppinfo->parent, dirSize, ppinfo->parent[0].location);
    
    // Add debugging to verify the write
    printf("DEBUG: fileWrite returned %d (expected %d)\n", writeResult, dirSize);
    
    free(ppinfo->parent);
    free(ppinfo);
    return 0;
}

// ----------------------------------------------------------------
// fs_mv
// This function moves a file or directory from one path to another.
// ----------------------------------------------------------------
/**
 * Validates path and retrieves directory entry information
 * @return 0 on success, -1 on failure
 */
int validateAndGetPathInfo(const char* pathname, PPRETDATA** ppinfo, int minIndexAllowed) {
    *ppinfo = malloc(sizeof(PPRETDATA));
    (*ppinfo)->parent = malloc(DE_SIZE);
    
    int res = parsePath(pathname, *ppinfo);
    int index = (*ppinfo)->lastElementIndex;
    
    if (res == -1 || index < minIndexAllowed) {
        free((*ppinfo)->parent);
        free(*ppinfo);
        return -1;
    }
    
    return 0;
}

/**
 * Finds or creates space for entry in destination directory
 * @return index of available slot, -1 on failure
 */
int findDestinationSlot(DE* destDir, const char* entryName) {
    int index = findInDir(destDir, (char*)entryName);
    
    if (index == -1) {
        // No existing entry with this name, find vacant space
        return find_vacant_space(destDir, (char*)entryName);
    } else if (destDir[index].location > 0) {
        // Entry exists and is in use, free its blocks
        if (returnFreeBlocks(destDir[index].location) == -1) {
            return -1;
        }
    }
    
    return index;
}

/**
 * Updates parent reference in directory being moved
 * @return 0 on success, -1 on failure
 */
int updateDirectoryParentRef(DE* entryToMove, DE* newParentDir) {
    if (!entryToMove->isDirectory) {
        return 0; // Nothing to do for non-directories
    }
    
    DE* dirContents = loadDir(entryToMove, 0);
    if (!dirContents) {
        return -1;
    }
    
    // Update parent (..) reference
    dirContents[1] = newParentDir[0];
    strncpy(dirContents[1].name, "..", DE_NAME_SIZE);
    
    int res = fileWrite(dirContents, calculateFormula(DE_SIZE, MINBLOCKSIZE), dirContents->location);
    free(dirContents);
    
    return (res == -1) ? -1 : 0;
}

/**
 * Move a file or directory from source to destination path
 */
int fs_mv(const char* sourcePath, const char* destPath) {
    PPRETDATA *sourceInfo = NULL;
    PPRETDATA *destInfo = NULL;
    DE *destDir = NULL;
    int result = 0;
    
    // Step 1: Validate source path (don't allow moving system directories)
    if (validateAndGetPathInfo(sourcePath, &sourceInfo, 2) != 0) {
        return -1;
    }
    
    // Step 2: Validate destination path (must be an existing directory)
    if (validateAndGetPathInfo(destPath, &destInfo, -1) != 0) {
        free(sourceInfo->parent);
        free(sourceInfo);
        return -1;
    }
    
    int destIndex = destInfo->lastElementIndex;
    if (destIndex == -1 || !destInfo->parent[destIndex].isDirectory) {
        result = -1;
        goto cleanup;
    }
    
    // Step 3: Load destination directory
    destDir = loadDir(destInfo->parent, destIndex);
    if (!destDir) {
        result = -1;
        goto cleanup;
    }
    
    // Step 4: Find slot in destination directory
    int slotIndex = findDestinationSlot(destDir, sourceInfo->lastElementName);
    if (slotIndex == -1) {
        result = -1;
        goto cleanup;
    }
    
    // Step 5: Copy the entry to destination
    int sourceIndex = sourceInfo->lastElementIndex;
    destDir[slotIndex] = sourceInfo->parent[sourceIndex];
    
    // Step 6: Update parent reference if moving a directory
    if (updateDirectoryParentRef(&destDir[slotIndex], destDir) != 0) {
        result = -1;
        goto cleanup;
    }
    
    // Step 7: Mark source entry as unused
    sourceInfo->parent[sourceIndex].location = -2;
    
    // Step 8: Write changes to disk
    if (fileWrite(destDir, calculateFormula(DE_SIZE, MINBLOCKSIZE), destDir->location) == -1 ||
        fileWrite(sourceInfo->parent, calculateFormula(DE_SIZE, MINBLOCKSIZE), sourceInfo->parent->location) == -1) {
        result = -1;
    }
    
cleanup:
    free(sourceInfo->parent);
    free(sourceInfo);
    free(destInfo->parent);
    free(destInfo);
    if (destDir) free(destDir);
    
    return result;
}