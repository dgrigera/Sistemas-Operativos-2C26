#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_INODES 256
#define MAX_NAME 64
#define MAX_PATH 256
#define MAX_DEPTH 16
#define MAX_DIR_ENTRIES 32
#define MAX_FILE_SIZE 4096

typedef enum { T_FREE, T_FILE, T_DIR, T_SYMLINK } inode_type_t;

typedef struct {
	char name[MAX_NAME];
	int inode;  // indice en la tabla de inodos
} dir_entry_t;

typedef struct {
	inode_type_t type;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	nlink_t nlink;
	off_t size;
	time_t atime;
	time_t mtime;
	time_t ctime;
	union {
		char file_data[MAX_FILE_SIZE];  // contenido del archivo
		struct {
			dir_entry_t entries[MAX_DIR_ENTRIES];
			int count;
		} dir;
	};
} inode_t;

typedef struct {
	inode_t inodes[MAX_INODES];
} fs_t;

// Inicializa la raiz (inodo 0, directorio, nlink 2) y marca el resto como libre.
void fs_init_empty(fs_t *fs);

// Devuelve el nro de inodo de path, o -ENOENT / -ENOTDIR / -ENAMETOOLONG.
int fs_lookup(fs_t *fs, const char *path);

// Devuelve el inodo del padre de path y deja el ultimo componente en name_out.
int fs_lookup_parent(fs_t *fs, const char *path, char *name_out);

// Llena st a partir del inodo de path. Devuelve 0 o un -errno.
int fs_getattr(fs_t *fs, const char *path, struct stat *st);

// Busca un inodo libre (T_FREE), lo inicializa con type, mode, uid, gid,
// los tres tiempos y nlink (1 si es archivo, 2 si es directorio).
// Devuelve el indice del inodo o -ENOSPC si no hay libres.
int inode_alloc(fs_t *fs, inode_type_t type, mode_t mode);

// Deja en cero el inodo idx y lo marca como libre (T_FREE).
void inode_free(fs_t *fs, int idx);

// Agrega la entrada name -> inode_idx al directorio dir_idx.
// Devuelve 0, -ENOTDIR si dir_idx no es directorio, -EEXIST si name ya existe,
// o -ENOSPC si el directorio esta lleno.
int dir_add_entry(fs_t *fs, int dir_idx, const char *name, int inode_idx);

// Quita la entrada name del directorio dir_idx y compacta el arreglo.
// Devuelve 0 o -ENOENT si name no existe.
int dir_remove_entry(fs_t *fs, int dir_idx, const char *name);

// Crea un directorio en path. Actualiza nlink y tiempos del padre.
int fs_mkdir(fs_t *fs, const char *path, mode_t mode);

// Elimina un directorio vacio en path. Actualiza nlink y tiempos del padre.
int fs_rmdir(fs_t *fs, const char *path);

// Carga en entries los nombres de las entradas del directorio,
// la cantidad en count_out y actualiza el atime del directorio.
int fs_readdir(fs_t *fs,
               const char *path,
               char entries[][MAX_NAME],
               int max_entries,
               int *count_out);

// Actualiza atime y mtime del inodo en path y setea ctime al tiempo actual.
int fs_utimens(fs_t *fs, const char *path, time_t atime, time_t mtime);

#endif