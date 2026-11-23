#define _DEFAULT_SOURCE
#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "fs.h"
#include "fs_files.h"
#include "persist.h"

#define DEFAULT_FILE_DISK "persistence_file.fisopfs"

// absolute path for persistence file used in
// `.init` and `.destroy` FUSE operations
static char filedisk_path[2 * PATH_MAX];
static fs_t fs;

static int
fisopfs_getattr(const char *path, struct stat *st)
{
	printf("[debug] fisopfs_getattr - path: %s\n", path);
	return fs_getattr(&fs, path, st);
}

static int
fisopfs_readdir(const char *path,
                void *buffer,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	printf("[debug] fisopfs_readdir - path: %s\n", path);

	char entries[MAX_DIR_ENTRIES + 2][MAX_NAME];
	int count = 0;

	int res = fs_readdir(&fs, path, entries, MAX_DIR_ENTRIES + 2, &count);
	if (res < 0)
		return res;

	for (int i = 0; i < count; i++) {
		filler(buffer, entries[i], NULL, 0);
	}

	return 0;
}

static int
fisopfs_mkdir(const char *path, mode_t mode)
{
	printf("[debug] fisopfs_mkdir - path: %s, mode: %o\n", path, mode);
	return fs_mkdir(&fs, path, mode);
}

static int
fisopfs_rmdir(const char *path)
{
	printf("[debug] fisopfs_rmdir - path: %s\n", path);
	return fs_rmdir(&fs, path);
}

static int
fisopfs_utimens(const char *path, const struct timespec tv[2])
{
	printf("[debug] fisopfs_utimens - path: %s\n", path);

	time_t now = time(NULL);
	time_t atime = now;
	time_t mtime = now;

	if (tv != NULL) {
		if (tv[0].tv_nsec == UTIME_OMIT)
			atime = (time_t) -1;
		else if (tv[0].tv_nsec != UTIME_NOW)
			atime = tv[0].tv_sec;

		if (tv[1].tv_nsec == UTIME_OMIT)
			mtime = (time_t) -1;
		else if (tv[1].tv_nsec != UTIME_NOW)
			mtime = tv[1].tv_sec;
	}

	return fs_utimens(&fs, path, atime, mtime);
}

static int
fisopfs_read(const char *path,
             char *buffer,
             size_t size,
             off_t offset,
             struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_read - path: %s, offset: %ld, size: %zu\n",
	       path,
	       (long) offset,
	       size);
	return fs_read(&fs, path, buffer, size, offset);
}

static int
fisopfs_write(const char *path,
              const char *buffer,
              size_t size,
              off_t offset,
              struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_write - path: %s, offset: %ld, size: %zu\n",
	       path,
	       (long) offset,
	       size);
	return fs_write(&fs, path, buffer, size, offset);
}

static int
fisopfs_truncate(const char *path, off_t size)
{
	printf("[debug] fisopfs_truncate - path: %s, size: %ld\n",
	       path,
	       (long) size);
	return fs_truncate(&fs, path, size);
}

// al montar intento cargar el archivo de persistencia
static void *
fisopfs_init(struct fuse_conn_info *conn)
{
	printf("[debug] fisopfs_init - filedisk_path: %s\n", filedisk_path);

	int res = persist_load(filedisk_path, &fs, sizeof(fs));

	if (res < 0) {
		// si no habia archivo es la primera vez y arranco vacio
		// si estaba roto aviso y tambien arranco vacio
		if (res != -ENOENT)
			fprintf(stderr,
			        "[debug] fisopfs_init - archivo invalido\n");

		fs_init_empty(&fs);
	}
	// sin datos privados por ahora
	return NULL;
}

// aca va a ir el guardado de todo al archivo
static void
fisopfs_destroy(void *private_data)
{
	printf("[debug] fisopfs_destroy\n");

	if (persist_save(filedisk_path, &fs, sizeof(fs)) < 0)
		fprintf(stderr, "[debug] fisopfs_destroy - no pude guardar\n");
}

// fuse la llama cuando se cierra un archivo y tambien guarda
static int
fisopfs_flush(const char *path, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_flush - path: %s\n", path);

	// si falla devuelvo EIO para que se entere quien cerro el archivo
	if (persist_save(filedisk_path, &fs, sizeof(fs)) < 0)
		return -EIO;
	// 0 es que salio bien
	return 0;
}

static int
fisopfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_create - path: %s, mode: %o\n", path, mode);
	return fs_create(&fs, path, mode);
}

static int
fisopfs_unlink(const char *path)
{
	printf("[debug] fisopfs_unlink - path: %s\n", path);
	return fs_unlink(&fs, path);
}

static int
fisopfs_link(const char *from, const char *to)
{
	printf("[debug] fisopfs_link - from: %s, to: %s\n", from, to);
	return fs_link(&fs, from, to);
}

static int
fisopfs_symlink(const char *target, const char *linkpath)
{
	printf("[debug] fisopfs_symlink - target: %s, linkpath: %s\n",
	       target,
	       linkpath);
	return fs_symlink(&fs, target, linkpath);
}

static int
fisopfs_readlink(const char *path, char *buf, size_t size)
{
	printf("[debug] fisopfs_readlink - path: %s\n", path);
	return fs_readlink(&fs, path, buf, size);
}

static struct fuse_operations operations = {
	.getattr = fisopfs_getattr,
	.readdir = fisopfs_readdir,
	.mkdir = fisopfs_mkdir,
	.rmdir = fisopfs_rmdir,
	.utimens = fisopfs_utimens,
	.read = fisopfs_read,
	.write = fisopfs_write,
	.truncate = fisopfs_truncate,
	.create = fisopfs_create,
	.unlink = fisopfs_unlink,
	.link = fisopfs_link,
	// estas son las de persistencia
	.init = fisopfs_init,
	.destroy = fisopfs_destroy,
	.flush = fisopfs_flush,
	.symlink = fisopfs_symlink,
	.readlink = fisopfs_readlink,
};

int
main(int argc, char *argv[])
{
	char *filedisk_name = DEFAULT_FILE_DISK;

	// DO NOT modify the following code section
	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], "--filedisk") == 0) {
			filedisk_name = argv[i + 1];

			// we remove the argument so that FUSE doesn't use our
			// argument or name as folder.
			//
			// equivalent to a pop.
			for (int j = i; j < argc - 1; j++) {
				argv[j] = argv[j + 2];
			}

			argc = argc - 2;
			break;
		}
	}

	// handle absolute path for persistence file
	// so background executions can work properly
	//
	// Hint: use `getcwd(3)` before `fuse_main`
	//

	// si ya empieza con / es absoluta y la copio tal cual
	if (filedisk_name[0] == '/') {
		snprintf(filedisk_path, sizeof(filedisk_path), "%s", filedisk_name);
	} else {
		// si no le pego adelante el directorio actual
		char cwd[PATH_MAX];

		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			perror("[debug] getcwd");
			return 1;
		}

		snprintf(filedisk_path,
		         sizeof(filedisk_path),
		         "%s/%s",
		         cwd,
		         filedisk_name);
	}

	return fuse_main(argc, argv, &operations, NULL);
}
