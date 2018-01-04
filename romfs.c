#include "romfs.h"

extern char __romfs_start;  //a linker has to provide this value which starts a romfs
void *romfs_image_addr = &__romfs_start;

static struct FD file_record[ROMFS_DRIVER_FD_TABLE_SIZE];

static int find_free_fd() {
    int i;
    for (i = 0; i < ROMFS_DRIVER_FD_TABLE_SIZE; ++i)
        if (file_record[i].data_ptr_begin == 0)
            return i;
    return -1;
}

static unsigned adjust_fs_ptr(const char *data_ptr) {
    unsigned offset = 0;
    while (*(++offset + data_ptr) != '\0');
    offset = (offset + 16) & ~15U;
    return offset;
}

static uint32_t le(uint32_t v) {//convert the value of v to little endian
#if ROMFS_DRIVER_BE
    return v;
#else
    unsigned l1 = (v >> 8) & 255;
    unsigned l2 = (v >> 16) & 255;
    return (v >> 24) + (l2 << 8) + (l1 << 16) + ((v & 255) << 24);
#endif
}

static int seek_fname(const char *data_ptr, unsigned offset, const char *fn) {
    unsigned ftype, next, diroffset = offset;
    int pos;
    for (;;) {
        const char *cptr = data_ptr + offset;
        ftype = le(*((uint32_t*)(cptr))) & 7;
        next = le(*((uint32_t*)(cptr))) & ~(uint32_t)15;
        while (fn[0] == '/') ++fn;
        if (strcmp(cptr + 16, fn) == 0)
           if (ftype == 0) //hard link
               return le(*((uint32_t*)(cptr + 4))) & ~(uint32_t)15;
           else if (ftype == 2) //normal file
               return offset;
           else if (ftype == 3) //symbolic link
               if (*(adjust_fs_ptr(cptr + 16) + cptr + 16) == '/')
                   return seek_fname(data_ptr, adjust_fs_ptr(romfs_image_addr + 16) + 16, adjust_fs_ptr(cptr + 16) + cptr + 16);
               else
                   return seek_fname(data_ptr, diroffset, adjust_fs_ptr(cptr + 16) + cptr + 16);
           else
               return -2; //incorrect type; devices, sockets and fifos are not supported
        if (strstr(fn, cptr + 16) == fn && fn[strlen(cptr + 16)] == '/')
            if (ftype == 0) {//hard link to . or ..
                pos = seek_fname(data_ptr,
                    le(*((uint32_t*)(data_ptr + (le(*((uint32_t*)(cptr + 4))) & ~(uint32_t)15) + 4))) & ~(uint32_t)15,
                    fn + 2);
                if (pos > 0) return pos;
            }
            else if (ftype == 1) {//directory
                pos = seek_fname(data_ptr, le(*((uint32_t*)(cptr + 4))) & ~(uint32_t)15, fn + strlen(cptr + 16) + 1);
                if (pos > 0) return pos;
            }
            else if (ftype == 3) {//symbolic link to a directory
                const char *s1 = adjust_fs_ptr(cptr + 16) + cptr + 16, *s2 = fn + strlen(cptr + 16) + 1;
                char *s = malloc(strlen(s1) + strlen(s2) + 2);
                strcpy(s, s1);
                strcat(s, s2 - 1);
                if (*s == '/')
                    pos = seek_fname(data_ptr, adjust_fs_ptr(romfs_image_addr + 16) + 16, s);
                else
                    pos = seek_fname(data_ptr, diroffset, s);
                free(s);
                if (pos > 0) return pos;
            }
        if (next == 0)
            return -1; //not found
        offset = next;
    }
}

int romfs_close(int fd)
{
    if (file_record[fd].data_ptr_begin == 0)
        return errno = EBADF, -1;
    file_record[fd].data_ptr_begin = 0;
    return 0;
}

int romfs_open(const char *path, int flags, ...)
{
    int pos;
    if (strncmp("-rom1fs-", romfs_image_addr, 8) != 0)
        return errno = EIO, -1;
    pos = seek_fname(romfs_image_addr, adjust_fs_ptr(romfs_image_addr + 16) + 16, path);
    if (pos > 0) {
        int fd = find_free_fd();
        if (fd < 0) return fd;
        file_record[fd].size = le(*((uint32_t*)(romfs_image_addr + pos + 8)));
        file_record[fd].data_ptr_begin = romfs_image_addr + adjust_fs_ptr(romfs_image_addr + pos + 16) + 16 + pos;
        file_record[fd].current_offset = 0;
        return fd + 3;  //skips system FDs
    }
    return errno = ENOENT, -1;
}

int romfs_read(int fd, void *buf, size_t nbytes)
{
    int max_to_read;
    if (file_record[fd].data_ptr_begin == 0)
        return errno = EBADF, -1;
    max_to_read = file_record[fd].size - file_record[fd].current_offset;
    if (max_to_read < 0) return 0;
    if (max_to_read > nbytes) max_to_read = nbytes;
    memmove(buf, file_record[fd].data_ptr_begin + file_record[fd].current_offset, max_to_read);
    file_record[fd].current_offset += max_to_read;
    return max_to_read;
}

off_t romfs_lseek(int fd, off_t offset, int whence)
{
    if (file_record[fd].data_ptr_begin == 0)
        return errno = EBADF, -1;
    if (whence == SEEK_SET)
        return file_record[fd].current_offset = offset;
    if (whence == SEEK_CUR)
        return file_record[fd].current_offset += offset;
    if (whence == SEEK_END)
        return file_record[fd].current_offset = file_record[fd].size - offset;
    return errno = EBADF, -1;
}

#if ROMFS_DRIVER_TESTING
int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("USAGE: this-program a-file-path-in-romfs");
        return -3;
    }
    int fd = romfs_open(argv[1], 0);
    if (fd < 0)
        perror("can't open");
    else {
        char c, buf[80];
        int q;
        while (romfs_read(fd, &c, 1) == 1) printf("%c", c);
        puts("");
        romfs_lseek(fd, 1, SEEK_SET);
        q = romfs_read(fd, buf, 80);
        fwrite(buf, 1, q, stdout);
        puts("");
        if (romfs_close(fd) != 0) puts("close error");
    }
    return 0;
}
#endif
