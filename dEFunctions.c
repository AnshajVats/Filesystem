/**************************************************************
* Class::  CSC-415-02 Spring 2025
* Name:: Karina Alvarado Mendoza
* Student IDs:: 921299233
* GitHub-Name:: Karina-Krystal
* Group-Name:: Horse
* Project:: Basic File System
*
* File:: dEFunctions.c
*
* Description:: 
*
**************************************************************/

#include <mfs.h>

//function for isDir
int isDir(DirectoryEntry * entry) 
{
    return entry->isDir;
}

//function for isFile, if it returns 0 is file, 1 directory
int isFile(DirectoryEntry * entry) 
{
    return entry->isDir;
}
