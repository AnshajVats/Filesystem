# CSC415 Group Term Assignment - File System

**Course:** CSC415 Operating Systems  
**Semester:** Spring 2025  
**Project:** Basic File System  
**Team Name:** Horse  
**Team Members:**  
- Karina Alvarado Mendoza (921299233)  
- Wilmaire Mejia (922846038)  
- Anshaj Vats (923760991)  
- Ayesha Irum (923656653)  

---

## 📚 Assignment Purpose and Learning Outcomes

- Large scale project planning  
- Working in groups  
- Understanding file systems and low-level file operations  
- Implementing buffering, free space management, and persistence  
- Managing directory structures and file metadata  
- Writing modular and well-documented C code in Linux (Ubuntu VM)  
- Multi-phased software development  

---

## 🔧 Project Summary

This file system was implemented across **three milestones**:

### ✅ Milestone 1: Initialization and Formatting
- Implemented a Volume Control Block (VCB) to store core metadata.
- Initialized a bitmap-based free space management system.
- Created the root directory and stored it using a contiguous FAT-style system.

### ✅ Milestone 2: Directory & Path Navigation
- Implemented core directory commands: `mkdir`, `rmdir`, `opendir`, `readdir`, `closedir`.
- Supported full path parsing with relative/absolute path resolution.
- Added path traversal and directory metadata functions (`fs_isDir`, `fs_isFile`, etc.).

### ✅ Milestone 3: Buffered I/O & File Operations
- Added `b_open`, `b_read`, `b_write`, `b_seek`, `b_close`.
- Designed internal file control blocks with dynamic buffering.
- Integrated full `fsshell` CLI to test file creation, deletion, and navigation.

---

## 🧪 Features Implemented

| Command  | Status |
|----------|--------|
| `ls`     | ✅ ON   |
| `cd`     | ✅ ON   |
| `md`     | ✅ ON   |
| `pwd`    | ✅ ON   |
| `touch`  | ✅ ON   |
| `cat`    | ✅ ON   |
| `rm`     | ✅ ON   |
| `cp`     | ✅ ON   |
| `mv`     | ✅ ON   |
| `cp2fs`  | ✅ ON   |
| `cp2l`   | ✅ ON   |

---

## 🗂 Directory & File Interfaces

```c
// Directory Operations
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);
fdDir * fs_opendir(const char *pathname);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

// File Operations
b_io_fd b_open(char *filename, int flags);
int b_read(b_io_fd fd, char *buffer, int count);
int b_write(b_io_fd fd, char *buffer, int count);
int b_seek(b_io_fd fd, off_t offset, int whence);
int b_close(b_io_fd fd);

// Miscellaneous
char * fs_getcwd(char *pathbuffer, size_t size);
int fs_setcwd(char *pathname);
int fs_isFile(char *filename);
int fs_isDir(char *pathname);
int fs_delete(char *filename);
int fs_stat(const char *filename, struct fs_stat *buf);
