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

extern VCB *vcb; // This points to the volume control block (VCB) structure
char cwdname[50] = "/";

// mfs.c

int fs_mkdir(const char *pathname, mode_t mode) {
    if (!pathname || strlen(pathname) >= 32) return -1;

    char *pathCopy = strdup(pathname);
    parsepathInfo ppI;
    if (parsepath(pathCopy, &ppI) != 0) {
        free(pathCopy);
        return -1;
    }

    // Check if parent is valid
    if (ppI.index != -1) {
        printf("Directory already exists.\n");
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
        return -1;
    }

    // Validate parent directory
    if (!isDEaDir(ppI.parent)) {
        printf("Parent is not a directory.\n");
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
        return -1;
    }

    // Create new directory (using parent's metadata)
    DirectoryEntry *newDir = createDir(50, &ppI.parent[0]); // Parent's '.' entry
    if (!newDir) {
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
        return -1;
    }

    // Find free slot in parent directory
    int freeSlot = -1;
    int entryCount = ppI.parent[0].size / sizeof(DirectoryEntry);
    for (int i = 0; i < entryCount; i++) {
        if (ppI.parent[i].name[0] == '\0') {
            freeSlot = i;
            break;
        }
    }

    if (freeSlot == -1) {
        free(newDir);
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
        return -1; // Parent full
    }
    // Populate new entry in parent
    strncpy(ppI.parent[freeSlot].name, ppI.lastElement, 31);
    ppI.parent[freeSlot].name[31] = '\0'; // Force null termination
    ppI.parent[freeSlot].isDir = 1;
    ppI.parent[freeSlot].startBlock = newDir[0].startBlock;
    ppI.parent[freeSlot].size = newDir[0].size;
    time_t now = time(NULL);
    ppI.parent[freeSlot].created = ppI.parent[freeSlot].modified = now;

    // Write updated parent to disk
    int numBlocks = (ppI.parent[0].size + vcb->blockSize - 1) / vcb->blockSize;
    LBAwrite(ppI.parent, numBlocks, ppI.parent[0].startBlock);

    // Reload root if modified
    if (ppI.parent == alrLoadedRoot) {
        free(alrLoadedRoot);
        alrLoadedRoot = LoadDirectory(&ppI.parent[0]);
        ppI.parent = alrLoadedRoot; // Update pointer
    }


    printf("Created '%s' at block %lu.\n", ppI.lastElement, newDir[0].startBlock);

    free(newDir);
    if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
    free(pathCopy);
    return 0;
}

// ------------------------------
// fs_opendir – opens a directory and returns a pointer to it
// ------------------------------
fdDir *fs_opendir(const char *pathname) {
    parsepathInfo ppI;
    if (parsepath((char *)pathname, &ppI) != 0) {
        printf("Invalid path: %s\n", pathname);
        return NULL;
    }

    // If path is "/", return root directory entries
    if (strcmp(pathname, "/") == 0) {
        fdDir *dir = malloc(sizeof(fdDir));
        dir->dirEntries = LoadDirectory(alrLoadedRoot);
        dir->currentEntry = 0;
        return dir;
    }

    // Handle other paths
    if (ppI.index == -1 || !isDEaDir(&ppI.parent[ppI.index])) {
        printf("Not a directory: %s\n", pathname);
        if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
        free(ppI.lastElement);
        return NULL;
    }

    fdDir *dir = malloc(sizeof(fdDir));
    dir->dirEntries = LoadDirectory(&ppI.parent[ppI.index]);
    dir->currentEntry = 0;

    // Cleanup if parent was loaded temporarily
    if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
    free(ppI.lastElement);

    return dir;
}

// mfs.c
// -----------------------------------------------------------------------------
// fs_readdir: iterate one entry at a time out of dirp->dirEntries[]
// -----------------------------------------------------------------------------
struct fs_diriteminfo *fs_readdir(fdDir *dirp) {
    if (!dirp || !dirp->dirEntries) return NULL;

    // Calculate max entries using directory size
    int maxEntries = dirp->dirEntries[0].size / sizeof(DirectoryEntry);
    if (dirp->currentEntry >= maxEntries) return NULL;

    DirectoryEntry *de = &dirp->dirEntries[dirp->currentEntry];
    
    // Skip empty entries (name starts with '\0')
    while (de->name[0] == '\0' && dirp->currentEntry < maxEntries) {
        dirp->currentEntry++;
        de = &dirp->dirEntries[dirp->currentEntry];
    }

    if (dirp->currentEntry >= maxEntries) return NULL;

    static struct fs_diriteminfo di;
    strncpy(di.d_name, de->name, sizeof(di.d_name));

    dirp->currentEntry++;
    return &di;
}
// -----------------------------------------------------------------------------
// fs_isDir: return 1 if path names a directory, 0 otherwise
// -----------------------------------------------------------------------------
int fs_isDir(char *path) {
    parsepathInfo ppI;
    if (parsepath(path, &ppI) != 0) return 0;
    int ret = isDEaDir(&ppI.parent[ppI.index]);
    free(ppI.lastElement);
    if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
    return ret;
}
// -----------------------------------------------------------------------------
// fs_isFile: return 1 if path names a regular file, 0 otherwise
// -----------------------------------------------------------------------------


int fs_isFile(char *path) {
    parsepathInfo ppI;
    if (parsepath(path, &ppI) != 0) return 0;
    int ret = !isDEaDir(&ppI.parent[ppI.index]);
    free(ppI.lastElement);
    if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
    return ret;
}

// -----------------------------------------------------------------------------
// fs_stat: fill in fs_stat from the DirectoryEntry metadata
// -----------------------------------------------------------------------------
int fs_stat(const char *path, struct fs_stat *buf) {
    parsepathInfo ppI;
    if (parsepath((char *)path, &ppI) != 0) return -1;

    DirectoryEntry *de = &ppI.parent[ppI.index];
    buf->st_size = de->size;
    buf->st_blksize = vcb->blockSize;
    buf->st_blocks = (de->size + vcb->blockSize - 1) / vcb->blockSize;
    buf->st_accesstime = de->accessed;
    buf->st_modtime = de->modified;
    buf->st_createtime = de->created;

    free(ppI.lastElement);
    if (ppI.parent != alrLoadedRoot && ppI.parent != alrLoadedcwd) free(ppI.parent);
    return 0;
}
// ------------------------------
// fs_getcwd – just returns "/" for now since we don't track actual path
// ------------------------------


char *fs_getcwd(char *pathname, size_t size) {
    if (!vcb || !alrLoadedcwd) {
        printf("[getcwd] Error: VCB/CWD not initialized.\n");
        return NULL;
    }

    // Check if we're at root (alrLoadedcwd points to root)
    if (alrLoadedcwd == alrLoadedRoot) {
        strncpy(cwdname, "/", sizeof(cwdname));
    }

    strncpy(pathname, cwdname, size);
    return pathname;
}


// mfs.c
int fs_closedir(fdDir *dirp) {
    if (dirp) {
        free(dirp->dirEntries); // Free the directory entries
        free(dirp);             // Free the directory pointer
    }
    return 0;
}


// -----------------------------------------------------------------------------
// setcwd – set the current working directory
// -----------------------------------------------------------------------------
int fs_setcwd(char *path) {
    if (!path) return -1;

    // 1) Duplicate so parsepath can strtok_r it:
    char *copy = strdup(path);
    if (!copy) return -1;

    // 2) Resolve via parsepath
    parsepathInfo ppI;
    if (parsepath(copy, &ppI) != 0) {
      free(copy);
      return -1;
    }

    // 3) Make sure it refers to a directory or root
    if (ppI.index == -1 ||
        (ppI.index >= 0 && !isDEaDir(&ppI.parent[ppI.index]))) {
      // cleanup
      free(ppI.lastElement);
      if (ppI.parent!=alrLoadedRoot && ppI.parent!=alrLoadedcwd) free(ppI.parent);
      free(copy);
      return -1;
    }

    // 4) Swap in the new cwd entries
    if (ppI.index == -2) {
      // “/” case
      if (alrLoadedcwd != alrLoadedRoot)
        free(alrLoadedcwd);
      alrLoadedcwd = alrLoadedRoot;
    } else {
      DirectoryEntry *d = &ppI.parent[ppI.index];
      DirectoryEntry *newcwd = LoadDirectory(d);
      if (!newcwd) {
        // cleanup...
        free(ppI.lastElement);
        if (ppI.parent!=alrLoadedRoot && ppI.parent!=alrLoadedcwd) free(ppI.parent);
        free(copy);
        return -1;
      }
      if (alrLoadedcwd != alrLoadedRoot)
        free(alrLoadedcwd);
      alrLoadedcwd = newcwd;
    }

    // 5) Update a global cwdname for fs_getcwd()
    if (strcmp(path, "/") == 0) {
      strcpy(cwdname, "/");
    } else if (path[0] == '/') {
      snprintf(cwdname, sizeof(cwdname), "%s", path);
    } else {
      // relative → append
      if (strcmp(cwdname, "/") == 0)
        snprintf(cwdname, sizeof(cwdname), "/%s", path);
      else
        snprintf(cwdname, sizeof(cwdname), "%s/%s", cwdname, path);
    }

    printf("[setcwd] Changing cwd to %s\n", cwdname);

    // 6) final cleanup
    free(ppI.lastElement);
    if (ppI.parent!=alrLoadedRoot && ppI.parent!=alrLoadedcwd) free(ppI.parent);
    free(copy);

    return 0;
}