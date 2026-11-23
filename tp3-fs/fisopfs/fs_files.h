#ifndef FS_FILES_H
#define FS_FILES_H

#include <sys/types.h>
#include "fs.h"

// sobre un inodo
int inode_read(inode_t *ino, char *buf, size_t size, off_t off);
int inode_write(inode_t *ino, const char *buf, size_t size, off_t off);
int inode_truncate(inode_t *ino, off_t new_size);

// por path
int fs_read(fs_t *fs, const char *path, char *buf, size_t size, off_t off);
int fs_write(fs_t *fs, const char *path, const char *buf, size_t size, off_t off);
int fs_truncate(fs_t *fs, const char *path, off_t size);

int fs_create(fs_t *fs, const char *path, mode_t mode);
int fs_unlink(fs_t *fs, const char *path);
int fs_link(fs_t *fs, const char *from, const char *to);
int fs_symlink(fs_t *fs, const char *target, const char *linkpath);
int fs_readlink(fs_t *fs, const char *path, char *buf, size_t size);

#endif
