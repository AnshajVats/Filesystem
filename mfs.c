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

        /* ====== mkdir Function ====== */

// This function is for creating a new directory at the given path.
// The idea is to walk through the path, figure out where to place the folder,
// actually make the folder, and then save that change into the parent directory.

int fs_mkdir(const char *targetPath, mode_t mode) {
    // First thing: break down the path so we know where we’re putting the new folder.
    PPRETDATA* pathDetails = parse_path(targetPath);
    if (!pathDetails) return -1;  // If the path is invalid or parsing failed, just quit.

    // Next: find a free slot in the parent directory for this new folder.
    // If there’s no room (directory is full), we give up.
    int slotIndex = find_and_validate_slot(pathDetails->parent, pathDetails->lastElementName, 1);
    if (slotIndex == -1) goto fail_cleanup;

    // Now let’s create space on disk for this new directory’s contents.
    // This is like reserving space for it in our virtual file system.
    int newDirBlock = createDirectory(DEFAULTDIRSIZE, pathDetails->parent);
    if (newDirBlock == -1) goto fail_cleanup;

    // Here we actually build the new directory entry (name, timestamps, block info, etc.)
    DE newFolderEntry;
    initialize_directory_entry(&newFolderEntry, pathDetails->lastElementName, 1, newDirBlock);

    // We finally plug the new entry into the parent directory and save the updated directory to disk.
    pathDetails->parent[slotIndex] = newFolderEntry;
    int writeResult = write_parent_directory(pathDetails->parent);

    // Clean up dynamically allocated memory before returning.
    free(pathDetails->parent);
    free(pathDetails);
    return writeResult;

fail_cleanup:
    // If something broke above, clean up whatever we’ve already allocated.
    if (pathDetails) {
        free(pathDetails->parent);
        free(pathDetails);
    }
    return -1;
}


/* ====== CWD Management Functions ====== */


// This function is used to simplify paths like "/folder/./../a" into a clean version like "/a/"
// It's useful so that our filesystem handles relative and redundant path references correctly.
char* cleanPath(char* pathname) {
    int maxParts = strlen(pathname) / 2;
    char** parts = malloc(sizeof(char*) * maxParts);
    char* token = strtok(pathname, "/");
    int actualSize = 0;

    // Split the path by '/' and store parts
    while (token != NULL) {
        parts[actualSize++] = strdup(token);
        token = strtok(NULL, "/");
    }

    // Process tokens: remove '.', handle '..'
    int* validIndices = malloc(sizeof(int) * actualSize);
    int cleanedIndex = 0;

    for (int i = 0; i < actualSize; i++) {
        if (strcmp(parts[i], ".") == 0) {
            continue;  // '.' means current directory, so skip
        } else if (strcmp(parts[i], "..") == 0) {
            if (cleanedIndex > 0) cleanedIndex--;  // '..' means go up one level
        } else {
            validIndices[cleanedIndex++] = i;  // Valid directory/file name
        }
    }

    // Reconstruct path
    char* cleaned = malloc(strlen(pathname));
    strcpy(cleaned, "/");
    for (int i = 0; i < cleanedIndex; i++) {
        strcat(cleaned, parts[validIndices[i]]);
        strcat(cleaned, "/");
    }

    for (int i = 0; i < actualSize; i++) free(parts[i]);
    free(parts);
    free(validIndices);

    return cleaned;
}

// We call this whenever we want to get the path of the current working directory
// This is used internally by the shell or user code that wants to see where we are
char* fs_getcwd(char* pathname, size_t size) {
    strncpy(pathname, cwdPathName, size);
    return cwdPathName;
}

// This sets the current working directory to the one the user asked for
// It validates, loads the directory, and then updates global `cwd` and `cwdPathName`
int fs_setcwd(char* pathname) {
    // Parse the path and prepare the parent directory
    PPRETDATA* ppinfo = malloc(sizeof(PPRETDATA));
    ppinfo->parent = malloc(DE_SIZE);
    int parseResult = parsePath(pathname, ppinfo);

    // If the path is root ("/"), just reset everything to the root directory
    if (ppinfo->lastElementIndex == -2) {
        cwd = loadDir(root, 0);
        strcpy(cwdPathName, "/");
        free(ppinfo->parent);
        free(ppinfo);
        return 0;
    }

    // If the path is invalid or not found, return an error
    if (parseResult == -1 || ppinfo->lastElementIndex == -1) {
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }

    // Load the directory we're switching into using the last element index
    DE* targetDir = loadDir(ppinfo->parent, ppinfo->lastElementIndex);
    if (!targetDir || targetDir->isDirectory != 1) {
        if (targetDir) free(targetDir);
        free(ppinfo->parent);
        free(ppinfo);
        return -1;
    }

    // If it's valid, save the target directory structure as our current one
    memcpy(cwd, targetDir, DE_SIZE);
    free(targetDir);

    // Handle absolute vs relative pathname updating
    if (pathname[0] == '/') {
        cwdPathName = strdup(pathname);  // Absolute path
    } else {
        strcat(cwdPathName, pathname);  // Relative path, append to existing
    }

    // Clean up the path for consistency (remove ./, ../, etc.)
    cwdPathName = cleanPath(cwdPathName);

    // Write back changes to disk to ensure everything is saved
    int blocksToWrite = calculateFormula(cwd->size, MINBLOCKSIZE);
    fileWrite(cwd, blocksToWrite, cwd->location);

    free(ppinfo->parent);
    free(ppinfo);
    return 0;
}

// Utility function that returns the ceiling of i / j
int calculateFormula(int i, int j) {
    return (i + j - 1) / j;
}

// Extracts just the last segment of a path string, like filename or folder name
char* getLastElement(const char* path) {
    // Special case: if the path is just '/', return "."
    if (strlen(path) == 1 && path[0] == '/') return strdup(".");

    const char* lastSlash = strrchr(path, '/');
    if (!lastSlash) return strdup(path);

    // Handle trailing slash (e.g., "/folder/")
    if (*(lastSlash + 1) == '\0') {
        char* trimmed = strdup(path);
        trimmed[strlen(trimmed) - 1] = '\0';
        const char* newSlash = strrchr(trimmed, '/');
        char* result = newSlash ? strdup(newSlash + 1) : strdup(trimmed);
        free(trimmed);
        return result;
    }

    return strdup(lastSlash + 1);
}


/* ====== Directory Listing Functions (fs_opendir, fs_readdir) ====== */

// We open a directory stream so we can later read its contents (like how `ls` works).
fdDir* fs_opendir(const char* pathname) {
    // We start by preparing to parse the path (e.g., "/folder/sub")
    PPRETDATA* pathInfo = malloc(sizeof(PPRETDATA));
    pathInfo->parent = malloc(DE_SIZE);

    // We break the path into components so we can find the final directory
    if (parsePath(pathname, pathInfo) == -1) {
        free(pathInfo->parent);
        free(pathInfo);
        return NULL;
    }

    // We isolate the last element (e.g., "sub" in "/folder/sub")
    char* entryName = getLastElement(pathname);
    int entryIndex = findInDir(pathInfo->parent, entryName);

    // If the directory doesn't exist in its parent, return an error
    if (entryIndex == -1) {
        fprintf(stderr, "fs_opendir: %s not found\n", entryName);
        free(pathInfo->parent);
        free(pathInfo);
        return NULL;
    }

    // We now confirm that the located entry is actually a directory
    DE entry = pathInfo->parent[entryIndex];
    if (!entry.isDirectory) {
        fprintf(stderr, "%s is not a directory\n", pathname);
        free(pathInfo->parent);
        free(pathInfo);
        return NULL;
    }

    // We prepare and populate a directory stream object that we’ll use to iterate
    fdDir* dirStream = malloc(sizeof(fdDir));
    dirStream->d_reclen = calculateFormula(entry.size, vcb->blockSize);
    dirStream->dirEntryPosition = entryIndex;
    dirStream->dirEntryLocation = entry.location;
    dirStream->index = 0;

    // We also prepare a buffer for storing information about entries we read
    dirStream->di = malloc(sizeof(struct fs_diriteminfo));
    dirStream->di->d_reclen = dirStream->d_reclen;
    dirStream->di->fileType = entry.isDirectory;
    strcpy(dirStream->di->d_name, entryName);

    // Clean up what we no longer need after setting things up
    free(pathInfo->parent);
    free(pathInfo);
    return dirStream;
}


// We use this to read the next entry in the open directory stream one-by-one.
struct fs_diriteminfo* fs_readdir(fdDir* dirp) {
    // We first calculate how many blocks this directory spans
    int totalBlocks = calculateFormula(sizeof(DE) * DECOUNT, vcb->blockSize);
    DE* directoryEntries = malloc(totalBlocks * vcb->blockSize);

    // Load the directory content from disk into memory
    int readSuccess = fileRead(directoryEntries, totalBlocks, dirp->dirEntryLocation);
    if (readSuccess == -1) {
        fprintf(stderr, "Could not load directory entries\n");
        free(directoryEntries);
        return NULL;
    }

    DE currentEntry = directoryEntries[dirp->index];

    // We skip over any unused or deleted entries
    while (dirp->index < DECOUNT - 1 && currentEntry.location == -2L) {
        currentEntry = directoryEntries[++dirp->index];
    }

    // If we reach the end, return NULL to signal no more entries
    if (dirp->index == DECOUNT - 1) {
        free(directoryEntries);
        return NULL;
    }

    // Copy relevant info into our return buffer so the caller can see it
    dirp->di->d_reclen = dirp->d_reclen;
    dirp->di->fileType = currentEntry.isDirectory;
    strcpy(dirp->di->d_name, currentEntry.name);
    dirp->index++;  // Prepare for the next call

    free(directoryEntries);
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
int isEmpty(DE* dir) {
    if (!dir) return 0;
    
    // Use actual directory size instead of DECOUNT
    int entry_count = dir[0].size / sizeof(DE);
    for (int i = 2; i < entry_count; i++) {
        if (dir[i].location > 0) return 0;
    }
    return 1;
}

int fs_rmdir(const char *pathname) {
    PPRETDATA *ppinfo = NULL;
    DE *currDir = NULL;
    int result = -1;

    if (!(ppinfo = parse_path(pathname))) goto cleanup;
    if (ppinfo->lastElementIndex < 0) goto cleanup;

    DE* entry = &ppinfo->parent[ppinfo->lastElementIndex];
    if (!entry->isDirectory) goto cleanup;

    if (!(currDir = loadDir(entry, 0))) goto cleanup;
    if (!isEmpty(currDir)) {
        printf("Error: Directory not empty '%s'\n", pathname);
        goto cleanup;
    }

    if (returnFreeBlocks(entry->location) == -1) goto cleanup;
    
    entry->location = -2;
    result = write_parent_directory(ppinfo->parent);

cleanup:
    if (currDir) free(currDir);
    if (ppinfo) {
        free(ppinfo->parent);
        free(ppinfo);
    }
    return result;
}

//----------------------------------------------------------------
// fs_delete
// This function deletes a file at the specified path.
//-------------------------------------------------------------------
int fs_delete(char* filename) {
    PPRETDATA* ppinfo = parse_path(filename);
    if (!ppinfo || ppinfo->lastElementIndex < 0) return -1;

    DE* entry = &ppinfo->parent[ppinfo->lastElementIndex];
    
    // Validate file
    if (entry->isDirectory) {
        fprintf(stderr, "Cannot delete directory with fs_delete\n");
        goto cleanup;
    }

    // Free resources
    if (entry->size > 0 && returnFreeBlocks(entry->location) == -1) goto cleanup;
    entry->location = -2;

    // Write changes
    int result = write_parent_directory(ppinfo->parent);

cleanup:
    free(ppinfo->parent);
    free(ppinfo);
    return result;
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