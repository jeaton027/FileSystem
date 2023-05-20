#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

// struct is a global variable so info can access
struct superblock {
        int64_t signature;
        int16_t total_amt;
        int16_t root_dir;
        int16_t data_block;
        int16_t block_amt;
        int8_t block_num;
        // type can change later this is just an array for excess
        void* excess;
};
/* TODO: Phase 1 */

int fs_mount(const char *diskname)
{
        /* TODO: Phase 1 */
        // open the file on the disk
        int return_val = block_disk_open(diskname);
        // if output is -1 return -1
        // otherwise continue getting info
        if (return_val < 0) {
                return -1;
        }

        // declare a void* buffer
        // probably need to malloc the buffer and delete after
        void* buffer;
        block_read(0, buffer);
        // will read data contents from superblock into buffer
        // read contents from buffer into struct
        // first 8 bytes read into entry 1
        struct superblock* sb = malloc(struct superblock*)sizeof(struct superblock);
        sb->excess = malloc(void*)sizeof(void);
}

int fs_umount(void)
{
        /* TODO: Phase 1 */
        // delete everything stored in struct
        // close
        // block_disk_close()
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

