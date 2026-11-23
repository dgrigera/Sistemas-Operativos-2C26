#define _DEFAULT_SOURCE

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "fs.h"

static int
dir_find(inode_t *dir, const char *name)
{
	for (int i = 0; i < dir->dir.count; i++) {
		if (strcmp(dir->dir.entries[i].name, name) == 0)
			return dir->dir.entries[i].inode;
	}
	return -1;
}

void
fs_init_empty(fs_t *fs)
{
	memset(fs, 0, sizeof(*fs));

	inode_t *root = &fs->inodes[0];
	root->type = T_DIR;
	root->mode = S_IFDIR | 0755;
	root->uid = getuid();
	root->gid = getgid();
	root->nlink = 2;
	root->atime = root->mtime = root->ctime = time(NULL);
}

int
fs_lookup(fs_t *fs, const char *path)
{
	if (strlen(path) >= MAX_PATH)
		return -ENAMETOOLONG;

	if (strcmp(path, "/") == 0)
		return 0;

	char copy[MAX_PATH];
	strcpy(copy, path);

	int cur = 0;
	int depth = 0;
	char *saveptr;

	for (char *tok = strtok_r(copy, "/", &saveptr); tok != NULL;
	     tok = strtok_r(NULL, "/", &saveptr)) {
		if (++depth > MAX_DEPTH)
			return -ENAMETOOLONG;
		if (strlen(tok) >= MAX_NAME)
			return -ENAMETOOLONG;
		if (fs->inodes[cur].type != T_DIR)
			return -ENOTDIR;

		cur = dir_find(&fs->inodes[cur], tok);
		if (cur < 0)
			return -ENOENT;
	}

	return cur;
}

int
fs_lookup_parent(fs_t *fs, const char *path, char *name_out)
{
	if (strlen(path) >= MAX_PATH)
		return -ENAMETOOLONG;

	if (strcmp(path, "/") == 0)
		return -EINVAL;

	const char *slash = strrchr(path, '/');
	size_t parent_len = slash - path;

	if (strlen(slash + 1) >= MAX_NAME)
		return -ENAMETOOLONG;
	strcpy(name_out, slash + 1);

	char parent[MAX_PATH];
	if (parent_len == 0) {
		strcpy(parent, "/");
	} else {
		memcpy(parent, path, parent_len);
		parent[parent_len] = '\0';
	}

	return fs_lookup(fs, parent);
}

int
fs_getattr(fs_t *fs, const char *path, struct stat *st)
{
	int inode = fs_lookup(fs, path);
	if (inode < 0) {
		return inode;
	}

	inode_t *inode_ptr = &fs->inodes[inode];

	memset(st, 0, sizeof(*st));

	st->st_mode = inode_ptr->mode;
	st->st_nlink = inode_ptr->nlink;
	st->st_uid = inode_ptr->uid;
	st->st_gid = inode_ptr->gid;
	st->st_size = inode_ptr->size;
	st->st_atime = inode_ptr->atime;
	st->st_mtime = inode_ptr->mtime;
	st->st_ctime = inode_ptr->ctime;

	return 0;
}

int
inode_alloc(fs_t *fs, inode_type_t type, mode_t mode)
{
	for (int i = 0; i < MAX_INODES; i++) {
		if (fs->inodes[i].type == T_FREE) {
			inode_t *ino = &fs->inodes[i];
			memset(ino, 0, sizeof(*ino));
			ino->type = type;
			ino->mode = mode;
			ino->uid = getuid();
			ino->gid = getgid();
			ino->nlink = (type == T_DIR) ? 2 : 1;
			ino->atime = ino->mtime = ino->ctime = time(NULL);
			return i;
		}
	}

	return -ENOSPC;
}

void
inode_free(fs_t *fs, int idx)
{
	memset(&fs->inodes[idx], 0, sizeof(fs->inodes[idx]));
}

int
dir_add_entry(fs_t *fs, int dir_idx, const char *name, int inode_idx)
{
	inode_t *dir = &fs->inodes[dir_idx];

	if (dir->type != T_DIR)
		return -ENOTDIR;
	if (dir_find(dir, name) != -1)
		return -EEXIST;
	if (dir->dir.count >= MAX_DIR_ENTRIES)
		return -ENOSPC;

	strcpy(dir->dir.entries[dir->dir.count].name, name);
	dir->dir.entries[dir->dir.count].inode = inode_idx;
	dir->dir.count++;

	return 0;
}

int
dir_remove_entry(fs_t *fs, int dir_idx, const char *name)
{
	inode_t *dir = &fs->inodes[dir_idx];
	int idx = -1;

	for (int i = 0; i < dir->dir.count; i++) {
		if (strcmp(dir->dir.entries[i].name, name) == 0) {
			idx = i;
			break;
		}
	}

	if (idx < 0)
		return -ENOENT;

	memmove(&dir->dir.entries[idx],
	        &dir->dir.entries[idx + 1],
	        (dir->dir.count - idx - 1) * sizeof(dir_entry_t));
	dir->dir.count--;

	return 0;
}

static int
path_depth(const char *path)
{
	int depth = 0;

	for (const char *p = path; *p != '\0'; p++) {
		if (*p == '/' && *(p + 1) != '\0' && *(p + 1) != '/')
			depth++;
	}

	return depth;
}

int
fs_mkdir(fs_t *fs, const char *path, mode_t mode)
{
	if (strcmp(path, "/") == 0)
		return -EEXIST;

	if (path_depth(path) > MAX_DEPTH)
		return -ENAMETOOLONG;

	char name[MAX_NAME];
	int parent_idx = fs_lookup_parent(fs, path, name);

	if (parent_idx < 0)
		return parent_idx;

	inode_t *parent = &fs->inodes[parent_idx];

	if (parent->type != T_DIR)
		return -ENOTDIR;

	// inode_alloc ya setea nlink = 2, uid, gid y atime/mtime/ctime = now
	int new_idx = inode_alloc(fs, T_DIR, S_IFDIR | (mode & 0777));
	if (new_idx < 0)
		return new_idx;

	int res = dir_add_entry(fs, parent_idx, name, new_idx);
	if (res < 0) {
		inode_free(fs, new_idx);
		return res;
	}

	// El nuevo subdirectorio aporta un link ("..") al directorio padre
	parent->nlink++;
	time_t now = time(NULL);
	parent->mtime = now;
	parent->ctime = now;

	return 0;
}

int
fs_rmdir(fs_t *fs, const char *path)
{
	if (strcmp(path, "/") == 0)
		return -EBUSY;

	char name[MAX_NAME];
	int parent_idx = fs_lookup_parent(fs, path, name);

	if (parent_idx < 0)
		return parent_idx;

	inode_t *parent = &fs->inodes[parent_idx];

	if (parent->type != T_DIR)
		return -ENOTDIR;

	int target_idx = dir_find(parent, name);

	if (target_idx < 0)
		return -ENOENT;

	inode_t *target = &fs->inodes[target_idx];

	if (target->type != T_DIR)
		return -ENOTDIR;
	if (target->dir.count > 0)
		return -ENOTEMPTY;

	int res = dir_remove_entry(fs, parent_idx, name);

	if (res < 0)
		return res;

	parent->nlink--;
	time_t now = time(NULL);
	parent->mtime = now;
	parent->ctime = now;

	inode_free(fs, target_idx);

	return 0;
}

int
fs_readdir(fs_t *fs,
           const char *path,
           char entries[][MAX_NAME],
           int max_entries,
           int *count_out)
{
	int idx = fs_lookup(fs, path);

	if (idx < 0)
		return idx;

	inode_t *dir = &fs->inodes[idx];

	if (dir->type != T_DIR)
		return -ENOTDIR;

	int n = 0;

	if (n < max_entries)
		strcpy(entries[n++], ".");
	if (n < max_entries)
		strcpy(entries[n++], "..");

	for (int i = 0; i < dir->dir.count && n < max_entries; i++) {
		strcpy(entries[n++], dir->dir.entries[i].name);
	}

	dir->atime = time(NULL);
	*count_out = n;

	return 0;
}

int
fs_utimens(fs_t *fs, const char *path, time_t atime, time_t mtime)
{
	int idx = fs_lookup(fs, path);

	if (idx < 0)
		return idx;

	inode_t *ino = &fs->inodes[idx];
	ino->atime = atime;
	ino->mtime = mtime;
	ino->ctime = time(NULL);

	return 0;
}
