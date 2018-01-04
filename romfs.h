#ifndef ROMFS_DRIVER_H
#define ROMFS_DRIVER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ROMFS_DRIVER_BE 0  //set to 1 if your system is Big Endian
#define ROMFS_DRIVER_TESTING 1
#define ROMFS_DRIVER_FD_TABLE_SIZE 16

struct FD {
    unsigned size, current_offset;
    char *data_ptr_begin;
};

int romfs_close(int fd);
int romfs_open(const char *path, int flags, ...);
int romfs_read(int fd, void *buf, size_t nbytes);
off_t romfs_lseek(int fd, off_t offset, int whence);

#endif
