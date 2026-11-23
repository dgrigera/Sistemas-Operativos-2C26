#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include "persist.h"

// esto va al principio del archivo y sirve para chequear que sea valido
struct persist_header {
	uint64_t magic;
	uint64_t size;
};

int
persist_save(const char *path, const void *data, size_t size)
{
	struct persist_header header = { .magic = PERSIST_MAGIC, .size = size };

	FILE *file = fopen(path, "wb");
	if (file == NULL) {
		perror("[debug] persist_save - fopen");
		return -EIO;
	}

	// primero escribo el encabezado y despues los datos
	if (fwrite(&header, sizeof(header), 1, file) != 1 ||
	    fwrite(data, 1, size, file) != size) {
		perror("[debug] persist_save - fwrite");
		fclose(file);
		return -EIO;
	}

	if (fclose(file) != 0) {
		perror("[debug] persist_save - fclose");
		return -EIO;
	}

	return 0;
}

int
persist_load(const char *path, void *data, size_t size)
{
	struct persist_header header;

	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		// si no existe es la primera vez y no es un error grave
		if (errno == ENOENT)
			return -ENOENT;

		perror("[debug] persist_load - fopen");
		return -EIO;
	}

	if (fread(&header, sizeof(header), 1, file) != 1) {
		fprintf(stderr, "[debug] persist_load - encabezado incompleto\n");
		fclose(file);
		return -EINVAL;
	}

	// si el magic o el size no coinciden el archivo no sirve
	if (header.magic != PERSIST_MAGIC || header.size != size) {
		fprintf(stderr, "[debug] persist_load - archivo invalido\n");
		fclose(file);
		return -EINVAL;
	}

	if (fread(data, 1, size, file) != size) {
		fprintf(stderr, "[debug] persist_load - datos incompletos\n");
		fclose(file);
		return -EINVAL;
	}

	fclose(file);
	return 0;
}