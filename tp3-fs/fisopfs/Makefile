CC = gcc
CFLAGS := -ggdb3 -O2 -Wall -std=c11
CFLAGS += -Wno-unused-function -Wvla

# Flags for FUSE
LDLIBS := $(shell pkg-config fuse --cflags --libs)

# Name for the filesystem!
FS_NAME := fisopfs

all: $(FS_NAME)

$(FS_NAME): fisopfs.c persist.o fs.o fs_files.o
	$(CC) $(CFLAGS) fisopfs.c persist.o fs.o fs_files.o $(LDLIBS) -o $@

fs.o: fs.c fs.h
	$(CC) $(CFLAGS) -c fs.c -o $@
	
format: .clang-files .clang-format
	xargs -r clang-format -i <$<

build:
	./dock build

run:
	./dock run

exec:
	./dock exec

clean:
	rm -rf $(EXEC) *.o core vgcore.* $(FS_NAME)

test: $(FS_NAME)
	./tests/tests.sh
	./tests/archivos.sh
	./tests/persistencia.sh
	./tests/directorios.sh
	./tests/softlinks.sh

.PHONY: all build clean format build run exec test

fs_files.o: fs_files.c fs_files.h fs.h
	$(CC) $(CFLAGS) -c fs_files.c -o $@
