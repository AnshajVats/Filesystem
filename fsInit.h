#ifndef FSINIT_H
#define FSINIT_H

#include "mfs.h"  // Assuming DirectoryEntry is defined here

#define DE_NAME_SIZE 28
#define DEFAULTDIRSIZE 56
#define DE_SIZE DEFAULTDIRSIZE * sizeof(DE)
#define DECOUNT ((DE_SIZE) / sizeof(DE))

// Specifications for volume control block

/*
 * @brief
 * @param
 * @param
 * @return
 */
extern struct VCB *vcb;
extern struct DE *root;
extern struct DE *cwd;
extern char * cwdPathName;
extern int * freeSpaceMap;
int createDirectory(int numberOfEntries, struct DE *parent); 
int fileRead(void* buff, int numberOfBlocks, int location);
int fileWrite(void* buff, int numberOfBlocks, int location);
int returnFreeBlocks(int location);
int fileSeek(int location, int numberOfBlocks);
int allocateblock(uint64_t numberOfBlocks) ;

#endif