#ifndef FSINIT_H
#define FSINIT_H

#include "mfs.h"  // Assuming DirectoryEntry is defined here

DirectoryEntry *createDir(int entryCount, DirectoryEntry *parent);
int initFileSystem(uint64_t totalBlocks, uint64_t blockSize);
void exitFileSystem();

#endif