#ifndef PERSIST_H
#define PERSIST_H

#include <stddef.h>

// numero que identifica a un archivo de fisopfs (son las letras fisp)
#define PERSIST_MAGIC 0x46495350

int persist_save(const char *path, const void *data, size_t size);
int persist_load(const char *path, void *data, size_t size);

#endif