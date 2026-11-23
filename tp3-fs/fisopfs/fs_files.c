#define _DEFAULT_SOURCE

#include <errno.h>
#include <string.h>
#include <time.h>
#include "fs_files.h"

// Operaciones sobre los bytes de UN inodo (parte 2: archivos).
// No dependen de cómo se busque el path.

int
inode_read(inode_t *ino, char *buf, size_t size, off_t off)
{
	if (ino->type == T_DIR)
		return -EISDIR;
	if (off < 0)
		return -EINVAL;
	if (off >= ino->size)
		return 0;  // EOF
	if (off + (off_t) size > ino->size)
		size = ino->size - off;  // no leer más de lo que hay
	memcpy(buf, ino->file_data + off, size);
	ino->atime = time(NULL);
	return size;
}

int
inode_truncate(inode_t *ino, off_t new_size)
{
	if (ino->type == T_DIR)
		return -EISDIR;
	if (new_size < 0)
		return -EINVAL;
	if (new_size > MAX_FILE_SIZE)
		return -EFBIG;
	if (new_size > ino->size)  // agrandar: rellenar con ceros
		memset(ino->file_data + ino->size, 0, new_size - ino->size);
	ino->size = new_size;
	ino->mtime = ino->ctime = time(NULL);
	return 0;
}

int
inode_write(inode_t *ino, const char *buf, size_t size, off_t off)
{
	if (ino->type == T_DIR)
		return -EISDIR;
	if (off < 0)
		return -EINVAL;
	if (off + (off_t) size > MAX_FILE_SIZE)
		return -EFBIG;
	if (off > ino->size)  // offset pasado el final: hueco de ceros
		memset(ino->file_data + ino->size, 0, off - ino->size);
	memcpy(ino->file_data + off,
	       buf,
	       size);  // memcpy y no strcpy: soporta binario
	if (off + (off_t) size > ino->size)
		ino->size = off + size;  // el tamaño solo crece si hace falta
	ino->mtime = ino->ctime = time(NULL);
	return size;
}

// Versiones por path: buscan el inodo y delegan en las de arriba.
int
fs_read(fs_t *fs, const char *path, char *buf, size_t size, off_t off)
{
	int idx = fs_lookup(fs, path);
	if (idx < 0)
		return idx;
	return inode_read(&fs->inodes[idx], buf, size, off);
}

int
fs_write(fs_t *fs, const char *path, const char *buf, size_t size, off_t off)
{
	int idx = fs_lookup(fs, path);
	if (idx < 0)
		return idx;
	return inode_write(&fs->inodes[idx], buf, size, off);
}

int
fs_truncate(fs_t *fs, const char *path, off_t size)
{
	int idx = fs_lookup(fs, path);
	if (idx < 0)
		return idx;
	return inode_truncate(&fs->inodes[idx], size);
}

// Crear, borrar y linkear archivos (parte 2). Usan las primitivas de fs.c.

int
fs_create(fs_t *fs, const char *path, mode_t mode)
{
	int exists = fs_lookup(fs, path);
	if (exists >= 0)
		return -EEXIST;
	if (exists != -ENOENT)
		return exists;  // ENOTDIR, ENAMETOOLONG...

	char name[MAX_NAME];
	int parent = fs_lookup_parent(fs, path, name);
	if (parent < 0)
		return parent;
	if (fs->inodes[parent].type != T_DIR)
		return -ENOTDIR;

	int idx = inode_alloc(fs, T_FILE, S_IFREG | (mode & 07777));
	if (idx < 0)
		return idx;

	int res = dir_add_entry(fs, parent, name, idx);
	if (res < 0) {
		inode_free(fs, idx);  // no dejar un inodo huérfano
		return res;
	}

	fs->inodes[parent].mtime = fs->inodes[parent].ctime = time(NULL);
	return 0;
}

int
fs_unlink(fs_t *fs, const char *path)
{
	int idx = fs_lookup(fs, path);
	if (idx < 0)
		return idx;
	if (fs->inodes[idx].type == T_DIR)
		return -EISDIR;

	char name[MAX_NAME];
	int parent = fs_lookup_parent(fs, path, name);
	if (parent < 0)
		return parent;

	int res = dir_remove_entry(fs, parent, name);
	if (res < 0)
		return res;

	inode_t *ino = &fs->inodes[idx];
	if (ino->nlink > 0)
		ino->nlink--;
	ino->ctime = time(NULL);
	if (ino->nlink == 0)
		inode_free(fs, idx);  // recién al llegar a 0 se libera el inodo

	fs->inodes[parent].mtime = fs->inodes[parent].ctime = time(NULL);
	return 0;
}

int
fs_link(fs_t *fs, const char *from, const char *to)
{
	int src = fs_lookup(fs, from);
	if (src < 0)
		return src;
	if (fs->inodes[src].type == T_DIR)
		return -EPERM;  // no se permiten hard links a directorios

	int dst = fs_lookup(fs, to);
	if (dst >= 0)
		return -EEXIST;
	if (dst != -ENOENT)
		return dst;

	char name[MAX_NAME];
	int parent = fs_lookup_parent(fs, to, name);
	if (parent < 0)
		return parent;

	int res = dir_add_entry(fs, parent, name, src);
	if (res < 0)
		return res;

	fs->inodes[src].nlink++;
	fs->inodes[src].ctime = time(NULL);
	fs->inodes[parent].mtime = fs->inodes[parent].ctime = time(NULL);
	return 0;
}

// crea un soft link en linkpath que apunta a target
// el target se guarda como contenido del inodo
int
fs_symlink(fs_t *fs, const char *target, const char *linkpath)
{
	// el texto tiene que entrar en el inodo
	if (strlen(target) >= MAX_FILE_SIZE)
		return -ENAMETOOLONG;

	int exists = fs_lookup(fs, linkpath);
	if (exists >= 0)
		return -EEXIST;
	if (exists != -ENOENT)
		return exists;

	char name[MAX_NAME];
	int parent = fs_lookup_parent(fs, linkpath, name);
	if (parent < 0)
		return parent;
	if (fs->inodes[parent].type != T_DIR)
		return -ENOTDIR;

	int idx = inode_alloc(fs, T_SYMLINK, S_IFLNK | 0777);
	if (idx < 0)
		return idx;

	inode_t *ino = &fs->inodes[idx];
	memcpy(ino->file_data, target, strlen(target));
	ino->size = strlen(target);

	int res = dir_add_entry(fs, parent, name, idx);
	if (res < 0) {
		inode_free(fs, idx);
		return res;
	}

	fs->inodes[parent].mtime = fs->inodes[parent].ctime = time(NULL);
	return 0;
}

// devuelve a donde apunta el soft link
int
fs_readlink(fs_t *fs, const char *path, char *buf, size_t size)
{
	if (size == 0)
		return -EINVAL;

	int idx = fs_lookup(fs, path);
	if (idx < 0)
		return idx;

	inode_t *ino = &fs->inodes[idx];
	// si no es un link no hay nada que leer
	if (ino->type != T_SYMLINK)
		return -EINVAL;

	// dejo lugar para el \0 del final
	size_t len = ino->size;
	if (len >= size)
		len = size - 1;

	memcpy(buf, ino->file_data, len);
	buf[len] = '\0';
	ino->atime = time(NULL);
	return 0;
}
