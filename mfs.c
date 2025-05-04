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
* Description:: File system core methods
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

int returnFreeBlocks(long location);
char* cleanPath(char* pathname);
struct VCB *volumeControlBlock;

//----------------------------------------------------------------
// fs_mkdir - Creates a new directory at the specified path
//----------------------------------------------------------------
int fs_mkdir(const char *pathname, mode_t mode) {
    int status, freeSlot;
    char *newDirName;
    PPRETDATA *info = malloc(sizeof(PPRETDATA));
    DE *parentDir, *dirEntry = malloc(MINBLOCKSIZE);

    if (!info || !dirEntry) return -1;

    info->parent = malloc(DE_SIZE);
    if (parsePath(pathname, info) == -1) return -1;

    parentDir = info->parent;
    newDirName = info->lastElementName;
    freeSlot = find_vacant_space(parentDir, newDirName);
    if (freeSlot == -1) return -1;

    status = createDirectory(DEFAULTDIRSIZE, parentDir);
    if (status == -1) return -1;

    if (fileRead(dirEntry, 1, status) == -1) return -1;

    parentDir[freeSlot] = dirEntry[0];
    strncpy(parentDir[freeSlot].name, newDirName, DE_NAME_SIZE);
    fileWrite(parentDir, calculateFormula(parentDir->size, MINBLOCKSIZE), parentDir->location);

    return 0;
}

char *fs_getcwd(char *pathname, size_t size) {
    strncpy(pathname, cwdPathName, size);
    return cwdPathName;
}

int fs_setcwd(char *pathname) {
    PPRETDATA *info = malloc(sizeof(PPRETDATA));
    info->parent = malloc(DE_SIZE);
    int result = parsePath(pathname, info);

    if (info->lastElementIndex == -2) {
        cwd = loadDir(root, 0);
        strcpy(cwdPathName, "/");
        free(info->parent); free(info);
        return 0;
    }

    if (result == -1 || info->lastElementIndex == -1) {
        free(info->parent); free(info);
        return -1;
    }

    struct DE *dir = loadDir(info->parent, info->lastElementIndex);
    if (!dir || dir->isDirectory != 1) return -1;

    memcpy(cwd, dir, DE_SIZE);
    free(dir);

    cwdPathName = pathname[0] == '/' ? strdup(pathname) : strcat(cwdPathName, pathname);
    cwdPathName = cleanPath(cwdPathName);
    fileWrite(cwd, calculateFormula(cwd->size, MINBLOCKSIZE), cwd->location);
    return 0;
}

char* cleanPath(char* pathname) {
    char** tokens = malloc(sizeof(char*) * strlen(pathname) / 2);
    char* savePtr = NULL;
    char* token = strtok_r(pathname, "/", &savePtr);
    int count = 0, top = 0;

    while (token != NULL) {
        tokens[count++] = strdup(token);
        token = strtok_r(NULL, "/", &savePtr);
    }

    int* stack = malloc(sizeof(int) * count);
    for (int i = 0; i < count; i++) {
        if (strcmp(tokens[i], ".") == 0) continue;
        if (strcmp(tokens[i], "..") == 0) { if (top > 0) top--; }
        else stack[top++] = i;
    }

    char* result = malloc(strlen(pathname));
    strcpy(result, "/");
    for (int i = 0; i < top; i++) {
        strcat(result, tokens[stack[i]]);
        strcat(result, "/");
    }

    return result;
}

int calculateFormula(int a, int b) {
    return (a + b - 1) / b;
}

int fs_isDir(char *pathname) {
    PPRETDATA *info = malloc(sizeof(PPRETDATA));
    info->parent = malloc(DE_SIZE);
    if (parsePath(pathname, info) == -1 || info->lastElementIndex == -1) return 0;
    int isDir = info->parent[info->lastElementIndex].isDirectory;
    free(info->parent); free(info);
    return isDir;
}

int fs_isFile(char *filename) {
    PPRETDATA *info = malloc(sizeof(PPRETDATA));
    info->parent = malloc(DE_SIZE);
    if (parsePath(filename, info) == -1 || info->lastElementIndex == -1) return 0;
    int isFile = !info->parent[info->lastElementIndex].isDirectory;
    free(info->parent); free(info);
    return isFile;
}

int fs_stat(const char *path, struct fs_stat *buf) {
    PPRETDATA *ppinfo;

    if (parsePath((char *)path, &ppinfo) != 0) return -1;

    DE *entry = &ppinfo->parent[ppinfo->lastElementIndex];

    buf->st_size = entry->size;
    buf->st_blksize = volumeControlBlock->blockSize;
    buf->st_blocks = (entry->size + volumeControlBlock->blockSize -1)
                            / volumeControlBlock->blockSize;
    buf->st_createtime = entry->dateCreated;
    buf->st_modtime = entry->dateModified;
    buf->st_accesstime = entry->dateLastAccessed;

    free(ppinfo->parent);
    free(ppinfo);
    return 0;
}

fdDir* fs_opendir(const char* pathname) {
    PPRETDATA* info = malloc(sizeof(PPRETDATA));
    info->parent = malloc(DE_SIZE);

    if (parsePath(pathname, info) == -1 || info->lastElementIndex < 0) {
        free(info->parent); free(info);
        return NULL;
    }

    DE* dir = loadDir(info->parent, info->lastElementIndex);
    if (!dir || dir->isDirectory != 1) return NULL;

    fdDir* fd = malloc(sizeof(fdDir));
    fd->dirEntryPosition = 2;
    fd->d_reclen = DE_SIZE;
    fd->directoryStartLocation = dir->location;
    fd->dir = dir;

    return fd;
}

struct fs_diriteminfo* fs_readdir(fdDir* dirp) {
    if (!dirp || dirp->dirEntryPosition >= DECOUNT) return NULL;

    while (dirp->dirEntryPosition < DECOUNT) {
        DE entry = dirp->dir[dirp->dirEntryPosition++];
        if (entry.location > 0 && entry.name[0] != '\0') {
            struct fs_diriteminfo* item = malloc(sizeof(struct fs_diriteminfo));
            strncpy(item->d_name, entry.name, DE_NAME_SIZE);
            item->fileType = entry.isDirectory ? 'd' : 'f';
            item->d_reclen = sizeof(struct fs_diriteminfo);
            return item;
        }
    }
    return NULL;
}

int fs_closedir(fdDir* dirp) {
    if (!dirp) return -1;
    free(dirp->dir);
    free(dirp);
    return 0;
}
