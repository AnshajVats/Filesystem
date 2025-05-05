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
PathParseResult* parse_path(const char* path) {
    PathParseResult* info = safe_malloc(sizeof(PathParseResult));
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
    PathParseResult* pathDetails = parse_path(targetPath);
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
    PathParseResult* ppinfo = malloc(sizeof(PathParseResult));
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
    PathParseResult* pathInfo = malloc(sizeof(PathParseResult));
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

/* ====== File & Directory Info Functions (fs_stat, fs_closedir) ====== */

// We use this when we want to fetch metadata about a file or folder like size, timestamps, etc.
int fs_stat(const char* pathname, struct fs_stat* buf) {
    // Break path into parent + last element structure
    PathParseResult* pathInfo = malloc(sizeof(PathParseResult));
    pathInfo->parent = malloc(DE_SIZE);

    // Parse path and make sure it exists
    if (parsePath(pathname, pathInfo) == -1) {
        free(pathInfo->parent);
        free(pathInfo);
        return -1;
    }

    // Get last component name and its index
    char* targetName = getLastElement(pathname);
    int index = findInDir(pathInfo->parent, targetName);

    if (index == -1) {
        free(pathInfo->parent);
        free(pathInfo);
        return -1;
    }

    // Fill out the stat structure with metadata
    DE target = pathInfo->parent[index];
    buf->st_size = target.size;
    buf->st_blksize = vcb->blockSize;
    buf->st_blocks = calculateFormula(target.size, vcb->blockSize);
    buf->st_accesstime = target.dateLastAccessed;
    buf->st_modtime = target.dateModified;
    buf->st_createtime = target.dateCreated;

    // Clean up
    free(pathInfo->parent);
    free(pathInfo);
    return index;
}


// When we’re done reading through a directory, we use this to close it and free memory
int fs_closedir(fdDir* dirp) {
    if (!dirp) {
        fprintf(stderr, "Cannot close directory — pointer is NULL\n");
        return 0;
    }

    // Clean up memory used for the directory stream
    free(dirp);
    return 1;
}


/* ====== Directory Type & Deletion Functions (fs_isDir, fs_isFile, fs_rmdir, fs_delete) ====== */

// We use this to check if a given path refers to a directory.
// It helps us verify whether we can navigate into it or apply operations like rmdir.
int fs_isDir(char* pathname) {
    PathParseResult* pathInfo = malloc(sizeof(PathParseResult));
    pathInfo->parent = malloc(DE_SIZE);

    int parsed = parsePath(pathname, pathInfo);
    if (parsed == -1 || pathInfo->lastElementIndex < 0) {
        free(pathInfo->parent);
        free(pathInfo);
        return 0;  // Not a directory or path doesn't exist
    }

    int isDirectory = pathInfo->parent[pathInfo->lastElementIndex].isDirectory;
    free(pathInfo->parent);
    free(pathInfo);
    return isDirectory;
}

// We use this to check if the provided name in the current directory is a file.
// This is helpful to validate file operations like delete or open.
int fs_isFile(char* filename) {
    int index = findInDir(cwd, filename);
    if (index == -1) return -1;  // Not found in current directory
    return !cwd[index].isDirectory;  // Return true if not a directory
}


// We use this to check if a directory is empty (only has "." and ".." entries).
// It's required before we safely remove a directory.
int isEmpty(DE* dir) {
    if (!dir) return 0;

    int entryCount = dir[0].size / sizeof(DE);
    for (int i = 2; i < entryCount; i++) {
        if (dir[i].location > 0) return 0;  // Found something other than "." or ".."
    }
    return 1;
}


// We use this to remove a directory from the file system.
// This only succeeds if the directory is valid and completely empty.
int fs_rmdir(const char* pathname) {
    // Parse the path so we can locate the directory and its parent
    PathParseResult* pathInfo = parse_path(pathname);
    if (!pathInfo || pathInfo->lastElementIndex < 0) {
        if (pathInfo) {
            free(pathInfo->parent);
            free(pathInfo);
        }
        return -1;
    }

    // Get the directory entry we’re trying to remove
    DE* dirEntry = &pathInfo->parent[pathInfo->lastElementIndex];

    // If it’s not a directory, we can’t remove it
    if (!dirEntry->isDirectory) {
        free(pathInfo->parent);
        free(pathInfo);
        return -1;
    }

    // Load the contents of the directory
    DE* dirContents = loadDir(dirEntry, 0);
    if (!dirContents || !isEmpty(dirContents)) {
        // Can’t delete if not empty
        if (dirContents) free(dirContents);
        free(pathInfo->parent);
        free(pathInfo);
        return -1;
    }

    // Free the blocks used by the directory
    if (returnFreeBlocks(dirEntry->location) == -1) {
        free(dirContents);
        free(pathInfo->parent);
        free(pathInfo);
        return -1;
    }

    // Mark the directory entry as deleted
    dirEntry->location = -2;

    // Save the parent directory changes to disk
    int result = write_parent_directory(pathInfo->parent);

    // Clean up memory
    free(dirContents);
    free(pathInfo->parent);
    free(pathInfo);

    return result;
}


// We use this to delete a file from the file system.
// It cleans up the storage blocks and marks the entry as removed.
int fs_delete(char* filename) {
    PathParseResult* pathInfo = parse_path(filename);
    if (!pathInfo || pathInfo->lastElementIndex < 0) return -1;

    DE* fileEntry = &pathInfo->parent[pathInfo->lastElementIndex];

    // If it's a directory, we don't allow deletion via fs_delete.
    if (fileEntry->isDirectory) {
        fprintf(stderr, "Cannot delete directory with fs_delete\n");
        free(pathInfo->parent);
        free(pathInfo);
        return -1;
    }

    if (fileEntry->size > 0 && returnFreeBlocks(fileEntry->location) == -1) {
        free(pathInfo->parent);
        free(pathInfo);
        return -1;
    }

    fileEntry->location = -2;  // Mark the file entry as invalid
    int result = write_parent_directory(pathInfo->parent);

    free(pathInfo->parent);
    free(pathInfo);
    return result;
}

/* ====== File/Directory Move Function (fs_mv) ====== */

// We use this helper to validate a path and load its directory metadata.
// It simplifies checking both the parse result and last element index.
int validateAndGetPathInfo(const char* pathname, PathParseResult** pathInfo, int minIndexAllowed) {
    *pathInfo = malloc(sizeof(PathParseResult));
    (*pathInfo)->parent = malloc(DE_SIZE);

    int parsed = parsePath(pathname, *pathInfo);
    int index = (*pathInfo)->lastElementIndex;

    if (parsed == -1 || index < minIndexAllowed) {
        free((*pathInfo)->parent);
        free(*pathInfo);
        return -1;
    }

    return 0;
}

// We use this helper to find an available slot in the destination directory.
// If an entry already exists with the same name, we clean it up if necessary.
int findDestinationSlot(DE* destDir, const char* entryName) {
    int index = findInDir(destDir, (char*)entryName);

    if (index == -1) {
        return find_vacant_space(destDir, (char*)entryName);
    } else if (destDir[index].location > 0) {
        if (returnFreeBlocks(destDir[index].location) == -1) return -1;
    }

    return index;
}

// If we move a directory, we need to update its parent reference ("..")
// so it correctly points to the new parent location.
int updateDirectoryParentRef(DE* entry, DE* newParent) {
    if (!entry->isDirectory) return 0;

    DE* content = loadDir(entry, 0);
    if (!content) return -1;

    content[1] = newParent[0];
    strncpy(content[1].name, "..", DE_NAME_SIZE);

    int result = fileWrite(content, calculateFormula(DE_SIZE, MINBLOCKSIZE), content->location);
    free(content);

    return (result == -1) ? -1 : 0;
}

// We use this to move a file or folder from one location to another in the system.
// It supports moving into an existing folder and updates necessary metadata.
int fs_mv(const char* sourcePath, const char* destPath) {
    PathParseResult* srcInfo = NULL;
    PathParseResult* dstInfo = NULL;
    DE* dstDir = NULL;
    int result = 0;

    // First, we validate the source path
    if (validateAndGetPathInfo(sourcePath, &srcInfo, 2) != 0) return -1;

    // Then, we validate the destination path
    if (validateAndGetPathInfo(destPath, &dstInfo, -1) != 0) {
        free(srcInfo->parent);
        free(srcInfo);
        return -1;
    }

    // Destination must be a valid directory
    int dstIndex = dstInfo->lastElementIndex;
    if (dstIndex == -1 || !dstInfo->parent[dstIndex].isDirectory) {
        result = -1;
        goto cleanup;
    }

    // Load the contents of the destination directory
    dstDir = loadDir(dstInfo->parent, dstIndex);
    if (!dstDir) {
        result = -1;
        goto cleanup;
    }

    // Find a valid slot to place the moved entry
    int slotIndex = findDestinationSlot(dstDir, srcInfo->lastElementName);
    if (slotIndex == -1) {
        result = -1;
        goto cleanup;
    }

    // Copy the entry from source to destination directory
    int srcIndex = srcInfo->lastElementIndex;
    dstDir[slotIndex] = srcInfo->parent[srcIndex];

    // If it's a directory, we update its internal ".." pointer
    if (updateDirectoryParentRef(&dstDir[slotIndex], dstDir) != 0) {
        result = -1;
        goto cleanup;
    }

    // Mark the old location as unused
    srcInfo->parent[srcIndex].location = -2;

    // Save the changes to disk
    if (fileWrite(dstDir, calculateFormula(DE_SIZE, MINBLOCKSIZE), dstDir->location) == -1 ||
        fileWrite(srcInfo->parent, calculateFormula(DE_SIZE, MINBLOCKSIZE), srcInfo->parent->location) == -1) {
        result = -1;
    }

cleanup:
    if (srcInfo) {
        free(srcInfo->parent);
        free(srcInfo);
    }
    if (dstInfo) {
        free(dstInfo->parent);
        free(dstInfo);
    }
    if (dstDir) free(dstDir);

    return result;
}
