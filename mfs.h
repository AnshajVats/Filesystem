/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: mfs.h
*
* Description:: 
*	This is the file system interface.
*	This is the interface needed by the driver to interact with
*	your filesystem.
*
**************************************************************/


#ifndef _MFS_H
#define _MFS_H
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#include "fsInit.h"

#include <dirent.h>
#define FT_REGFILE   1 
#define FT_DIRECTORY 2 
#define FT_LINK      3 

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif


typedef struct {
    int bytesNeeded;
    int blockCount;
    int maxEntryCount;
} DirectorySizeInfo;

typedef struct VCB{
	uint64_t signature;         // Signature to identify the filesystem
    uint64_t blockSize;         // Size of each block in bytes
	uint64_t totalBlocks;       // Total number of blocks on disk
	uint64_t freeSpaceMap;
	uint64_t freeSpaceSize;     
	uint64_t rootDir;	   
	uint64_t rootSize;		
	uint64_t firstBlock;		   
	uint64_t totalFreeSpace;	   
}VCB;

// Specifications for directory entry
struct DE
{
	char name[DE_NAME_SIZE];
	int isDirectory;
	int location;
	int size;	
	time_t dateCreated;
	time_t dateModified;
	time_t dateLastAccessed;
};
typedef struct DE DE;


struct parsePathData{
    struct DE* parent;
    int lastElementIndex;
    char* lastElementName;
};

typedef struct parsePathData PathParseResult;

// This structure is returned by fs_readdir to provide the caller with information
// about each file as it iterates through a directory
struct fs_diriteminfo
	{
    unsigned short d_reclen;    /* length of this record */
    unsigned char fileType;    
    char d_name[256]; 			/* filename max filename is 255 characters */
	};

// This is a private structure used only by fs_opendir, fs_readdir, and fs_closedir
// Think of this like a file descriptor but for a directory - one can only read
// from a directory.  This structure helps you (the file system) keep track of
// which directory entry you are currently processing so that everytime the caller
// calls the function readdir, you give the next entry in the directory
typedef struct
	{
	/*****TO DO:  Fill in this structure with what your open/read directory needs  *****/
	unsigned short  d_reclen;			/* length of this record */
	unsigned short	dirEntryPosition;	/* which directory entry position, like file pos */
	unsigned long 	dirEntryLocation;	/* location of the first DE*/
	unsigned int index;					/* index of the current entry */
	struct DE *	directory;				/* Pointer to the loaded directory you want to iterate */
	struct fs_diriteminfo * di;			/* Pointer to the structure you return from read */
	} fdDir;


// Key directory functions
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);

// Directory iteration functions
fdDir * fs_opendir(const char *pathname);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

// Misc directory functions
char * fs_getcwd(char *pathname, size_t size);
int fs_setcwd(char *pathname);   //linux chdir
int fs_isFile(char * filename);	//return 1 if file, 0 otherwise
int fs_isDir(char * pathname);		//return 1 if directory, 0 otherwise
int fs_delete(char* filename);	//removes a file
int fs_mv(const char* startpathname, const char* endpathname); //moves a file
int calculateFormula(int i, int j);
char* getLastElement(const char* path);


// This is the strucutre that is filled in from a call to fs_stat
struct fs_stat
	{
	off_t     st_size;    		/* total size, in bytes */
	blksize_t st_blksize; 		/* blocksize for file system I/O */
	blkcnt_t  st_blocks;  		/* number of 512B blocks allocated */
	time_t    st_accesstime;   	/* time of last access */
	time_t    st_modtime;   	/* time of last modification */
	time_t    st_createtime;   	/* time of last status change */
	
	/* add additional attributes here for your file system */
	};

int fs_stat(const char *path, struct fs_stat *buf);

#endif