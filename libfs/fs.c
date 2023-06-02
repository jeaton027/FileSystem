#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 65535 //OXFFFF

/* TODO: Phase 1 */
// struct is a global variable so info can access
// __attribute__((packed)): no padding added b/t data elements
// data is exactly in order, by defined size
struct __attribute__((packed)) superblock {
  int64_t signature;
  int16_t total_amt;
  int16_t root_dir;
  int16_t data_block;
  int16_t block_amt;
  int8_t block_num;
  // type can change later this is just an array for excess
  int8_t excess[4079]; //TODO explict size needed
};

// define for every entry in root table
struct __attribute__((packed)) root {
  //char* filename;
  char filename[FS_FILENAME_LEN];

  int32_t size;
  uint16_t index;
  int8_t padding[10]; //TODO needs to be exact size of array
};

// read contents of root dir here
struct root * entry = NULL;

// global superblock pointer
struct superblock * sb;

/* Filename to fd table */
struct fdMap {
  int fd;
  char * filename;
  FILE * fp;
  int offset;
};

struct fdMap fd_table[BLOCK_SIZE];

//int16_t *FAT_ARRAY;
// /* read contents of root dir here */
// struct root* root_entry;

int fs_mount(const char * diskname) {
  /* TODO: Phase 1 */
  // TODO if disk is opened, but mount fails: returns -1, does disk need to be closed?
  // open the file on the disk
  int return_val = block_disk_open(diskname);
  // if output is -1 return -1
  // otherwise continue getting info
  if (return_val < 0) {
    return -1;
  }

  // declare a void* buffer
  void * buffer = malloc(BLOCK_SIZE);
  if (buffer == NULL) {
    return -1; // not properly allocated
  }

  // read into buffer
  if (block_read(0, buffer) < 0) {
    free(buffer);
    return -1; // failed to read
  }

  //TODO *** could read in root data elemnents here? everything but data blocks?

  // will read data contents from superblock into buffer
  // read contents from buffer into struct
  sb = malloc(sizeof(struct superblock));
  if (sb == NULL) {
    free(buffer);
    return -1;
  }
  memcpy(sb, buffer, sizeof(struct superblock)); // assuming that excess is at the end of struct
  /* Initialize <fd, filename> map */
  for (int i = 0; i < BLOCK_SIZE; i++) {
    fd_table[i].fd = -1;
    fd_table[i].filename = NULL;
    fd_table[i].fp = NULL;
    fd_table[i].offset = 0;
  }
  // Clean up
  //FAT_ARRAY = malloc(sizeof(int16_t) * sb->block_num);
  free(buffer);
  return 0;
} //mount

int fs_umount(void) {
  /* TODO: Phase 1 */
  // if disk was mounted
  if (sb == NULL) {
    return -1;
  }

  // write superblock back to disk
  void * buffer = malloc(BLOCK_SIZE);
  if (buffer == NULL) {
    return -1;
  }

  memcpy(buffer, sb, sizeof(struct superblock));

  if (block_write(0, buffer) < 0) {
    free(buffer);
    return -1;
  }

  // clean
  free(buffer);
  //free(sb->excess);//TODO fix
  free(sb);
  //free(FAT_ARRAY);
  sb = NULL;

  // close disk
  if (block_disk_close() < 0) {
    return -1;
  }

  return 0;
} //unmount

int fs_info(void) {
  /* TODO: Phase 1 */
  if (sb == NULL) {
    return -1;
  }
  // traverse FAT
  // FAT Index starts at block 1

  void * buffer = malloc(BLOCK_SIZE);
  block_read(1, buffer);

  // number of entries in FAT = sb->datablk

  // keep track of total amount of FAT entries

  int totalFat = 0;
  // keep track of number of open entries
  int openFATentries = 0;
  int cur_db = 1;
  void * fat_buf = malloc(BLOCK_SIZE * sb -> block_num);
  int offset = 0;
  for (int i = 1; i <= sb -> block_num; i++) {
    /* Read one FAT block at a time */
    void * fat_blk = malloc(BLOCK_SIZE);
    block_read(i, fat_blk);
    offset = (i - 1) * BLOCK_SIZE;
    /* Append to larger buffer */
    memcpy((char * ) fat_buf + offset, fat_blk, BLOCK_SIZE);
    /* Free memory */
    free(fat_blk);
  }
  /* Look at every 16 bits in FAT */
  //sb->data_block
  for (int i = cur_db; i <= sb -> block_amt; i++) {
    int16_t entry;
    offset = i * 2;
    memcpy( & entry, (int16_t * )((char * ) fat_buf + offset), sizeof(int16_t));
    /* FAT Entry is invalid if it is 0 */
    // printf("entry %d\n", entry);
    if (entry == 0) {
      /* Found next empty FAT Block */
      openFATentries++;
      totalFat++;
    } else {
      totalFat++;
    }
  }
  free(fat_buf);

  // read contents into buffer
  void * buf = malloc(BLOCK_SIZE);
  block_read(sb -> root_dir, buf);
  // check if root directory entry is filled
  int emptyrootspaces = 0;
  // counter total number entries
  int totalrootentries = 0;
  // traverse root directory
  // for all 128 entries read into entry
  for (int i = 0; i < 128; i++) {
    entry = malloc(sizeof(struct root));
    memcpy(entry, buf + i * sizeof(struct root), sizeof(struct root));
    // entry is empty
    // if (entry->filename == NULL) {
    if (entry -> filename[0] == '\0') {
      //printf("entry->filename= %s\n", entry->filename);
      emptyrootspaces++;
      totalrootentries++;
    } else {
      //printf("entry->filename[0]= %s\n", entry->filename);
      totalrootentries++;
    }
    free(entry);
  }

  //TODO: find right information to print
  printf("FS Info:\n");
  printf("total_blk_count=%hd\n", sb -> total_amt);
  printf("fat_blk_count=%d\n", sb -> block_num);
  printf("rdir_blk=%d\n", sb -> root_dir);
  printf("data_blk=%d\n", sb -> data_block);
  printf("data_blk_count=%d\n", sb -> block_amt);
  printf("fat_free_ratio=%d/%d\n", openFATentries - 1, totalFat);
  printf("rdir_free_ratio=%d/%d\n", emptyrootspaces, totalrootentries);
  //TODO emptyrootspaces not correct

  return 0;
} //info

int fs_create(const char * filename) {
  // check if sb mounted
  if (sb == NULL) {
    return -1;
  }
  void * buffer = malloc(BLOCK_SIZE);
  // read rootdir into buffer
  block_read(sb -> root_dir, buffer);
  // find empty entry in rootdir
  for (int i = 0; i < 128; i++) {
    struct root * entry = (struct root * )(buffer + i * sizeof(struct root));
    if (strcmp(entry -> filename, filename) == 0) {
      free(buffer);
      return -1; // File already exists
    }
    // memcopy from buffer into entry
    //memcpy(entry, buffer, sizeof(struct root));
    //printf("COMPARE %s : %s\n",entry->filename, filename);
    if (entry -> filename[0] == '\0') { // empty entry
      //entry->filename = malloc(strlen(filename + 1));
      strcpy(entry -> filename, filename);
      //entry->filename = filename;
      //******
      entry -> size = 0; // given to us
      entry -> index = FAT_EOC; //TODO is value of FAT_EOC correct? 

      // print checks here
      block_write(sb -> root_dir, buffer); //
      free(buffer);
      //free(entry->filename);
      //printf("COMPARE2 %s : %s\n",entry->filename, filename);
      //for(int i = 0; i <7; i++) {
      //  printf("try: %c\n",entry->filename[i]);
      //}
      return 0;
    }
  }
  free(buffer);
  return -1;
} //create

int fs_delete(const char * filename) {
  /* TODO: Phase 2 */
  // if disk was mounted
  if (sb == NULL) {
    return -1;
  }

  void * buffer = malloc(BLOCK_SIZE);
  block_read(sb -> root_dir, buffer);
  // look for file in rootdir
  for (int i = 0; i < 128; i++) {
    struct root * entry = (struct root * )(buffer + i * sizeof(struct root));
    if (strcmp(entry -> filename, filename) == 0) {
      // Store first block index
      uint16_t next_index = entry -> index;
      // Free blocks occupied by file
      // traverse until last index of FAT
      while (next_index != FAT_EOC) {
        if (next_index == 0) {
          return -1;
        }
        void * fat_buffer = malloc(BLOCK_SIZE);
        // read fatblock into buf
        // blocknum is starting blknum of FAT
        // blocksize / 2 , represents fat entry 2 bytes (half a block)
        // starting blocknum + (calculated offset)
        block_read(sb -> block_num + next_index / (BLOCK_SIZE / 2), fat_buffer);
        // pointer to next entry
        uint16_t * index = (uint16_t * )(fat_buffer + (next_index % (BLOCK_SIZE / 2)) * 2);
        uint16_t old_index = next_index;
        next_index = * index;
        // block is now empty
        * index = 0;
        // write fat block back into disk
        block_write(sb -> block_num + old_index / (BLOCK_SIZE / 2), fat_buffer);
        free(fat_buffer);
      }
      // After blocks are freed, mark the root entry as deleted
      entry -> filename[0] = '\0';
      entry -> size = 0;
      entry -> index = FAT_EOC;
      // write rootdir block back into disk
      block_write(sb -> root_dir, buffer);
      free(buffer);
      return 0;
    }
  }
  free(buffer);
  return -1;
} //delete


int fs_ls(void) {
  /* TODO: Phase 2 */
  if (sb == NULL) {
    return -1;
  }

  void * buffer = malloc(BLOCK_SIZE);
  if (buffer == NULL) {
    return -1;
  }
  // read from rootdir into buffer
  block_read(sb -> root_dir, buffer);

  printf("Fs Ls:\n");
  for (int i = 0; i < 128; i++) {
    struct root * entry = (struct root * )(buffer + i * sizeof(struct root));
    //
    //printf("entry->filename= %s\n", entry->filename);
    if (entry -> filename[0] != '\0') {
      printf("file: %s, size: %d, data_blk: %d\n", entry -> filename, entry -> size, entry -> index);

    }
  }

  free(buffer);
  return 0;
} //ls

int fs_open(const char * filename) {
  /* Return -1 if no disk is mounted */
  if (sb == NULL) {
    return -1;
  }

  /* Open file for reading and writing */
  FILE * fp = fopen(filename, "r+");
  int fd;
  /* Add fd, filename to fd_table */
  for (int i = 0; i < BLOCK_SIZE; i++) {
    if (fd_table[i].fd == -1) {
      fd = i;
      fd_table[i].fd = fd;
      fd_table[i].filename = (char * ) malloc(strlen(filename) + 1);
      strcpy(fd_table[i].filename, filename);
      fd_table[i].fp = fp;
      break;
    }
  }
  /* Only return valid fd */
  if (fd < 0) {
    return -1;
  }

  return fd;
} //open

int fs_close(int fd) {
  /* Return -1 if no disk is mounted or fd is invalid */
  if (sb == NULL || fd < 0) {
    return -1;
  }

  /* Remove fd, filename from fd_table */
  FILE * fp;

  for (int i = 0; i < BLOCK_SIZE; i++) {
    if (fd_table[i].fd == fd) {
      fp = fd_table[i].fp;
      fd_table[i].fd = -1;
      fd_table[i].filename = NULL;
      fd_table[i].fp = NULL;
    }
  }

  /* Close file */
  if (fclose(fp) != 0) {
    return -1;
  }

  return 0;
} //close

int fs_stat(int fd) {
  /* Return -1 if invalid fd */
  if (fd < 0) {
    return -1;
  }
  /* Look through table for filename */
  char * filename = '\0';
  for (int i = 0; i < BLOCK_SIZE; i++) {
    //printf("fd_table[i].fd %d\n", fd_table[i].fd);

    if (fd_table[i].fd == fd) {
      filename = (char * ) malloc(strlen(fd_table[i].filename) + 1);
      strcpy(filename, fd_table[i].filename);
      //printf("filename: %s\n", fd_table[i].filename);
    }
  }

  /* Read from root dir into buffer */
  void * buffer = malloc(BLOCK_SIZE);
  block_read(sb -> root_dir, buffer);

  /* Traverse root directory */
  for (int i = 0; i < 128; i++) {
    struct root * entry = (struct root * )(buffer + i * sizeof(struct root));
    //printf("entry->filename %s :: fn %s\n", entry->filename, filename);
    if (strcmp(entry -> filename, filename) == 0) {
      /* Return corresponding size to filename */
      return entry -> size;
    }
  }
  free(buffer);

  /* Fd is not an open file in the file table */
  return -1;
} //stat

int fs_lseek(int fd, size_t offset) {

  /* No FS is currently mounted */
  if (sb == NULL || fd < 0) {
    return -1;
  }

  /* Add offset to the fd table 
   * Get the file pointer */
  FILE * fp;

  for (int i = 0; i < BLOCK_SIZE; i++) {
    if (fd_table[i].fd == fd) {
      fd_table[i].offset = offset;
      fp = fd_table[i].fp;
    }
  }

  /* Check that file has been opened */
  if (fp == NULL) {
    return -1;
  }

  /* Set fp to its offset */
  fseek(fp, offset, SEEK_SET);

  return 0;
} //lseek

int fs_write(int fd, void *buf, size_t count) {
    if (sb == NULL || fd < 0) {
        return -1;
    }

    int cur_db = 1; 					// current data block index
    size_t data_written = 0; 	// data written so far
    int fat_index = -1; 			// current FAT index
    int prev_fat_index = -1;	// prev FAT index
		int first_index = -1;			// entry->index for file

		// given file descriptor, find corresponding pointer fp
		// if file is open
    FILE *fp = NULL;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (fd_table[i].fd == fd) {
            fp = fd_table[i].fp;
        }
    }

    if (fp == NULL || buf == NULL) {
        return -1;
    }

		// write until all data given is written, or until 
		// available blocks in disks run out
    while (cur_db < block_disk_count() && count > 0) {
				void* fat_buf = malloc(BLOCK_SIZE * sb->root_dir - 1);
        int offset = 0;

        // Read all FAT blocks into fat_buf
        for (int i = 1; i < sb->root_dir; i++) {
            void* fat_blk = malloc(BLOCK_SIZE);
            block_read(i, fat_blk);
            offset = (i - 1) * BLOCK_SIZE;
            memcpy((char*)fat_buf + offset, fat_blk, BLOCK_SIZE);
            free(fat_blk);
        }

				// iterate over every FAT entry
        int num_entries = (BLOCK_SIZE * (sb->root_dir - 1)) / 2;
        for (int i = cur_db; i < num_entries; i++) {
            int16_t entry;
            offset = i * 2;
						// fill entry with data from fat_buf using offset
            memcpy(&entry, (int16_t*)((char*)fat_buf + offset), sizeof(int16_t));

						// if empty, entry is filled
            if (entry == 0) {
                fat_index = i;
                //first index of file
								if (first_index == -1) {
                    first_index = fat_index;
                }
								entry = FAT_EOC; // Mark as end of chain initially
                memcpy((int16_t*)((char*)fat_buf + offset), &entry, sizeof(int16_t));

                // Update previous FAT entry with current index if exists
                if (prev_fat_index != -1) {
                    entry = fat_index; // prev entry to now point to this block
                    memcpy((int16_t*)((char*)fat_buf + prev_fat_index * 2), 
																				&entry, sizeof(int16_t));
                }

                // Write back the FAT block to the disk
                block_write(i / (BLOCK_SIZE / 2) + 1, fat_buf);
                cur_db = i;
                prev_fat_index = i;
                break;
            }
        }
        free(fat_buf);

				// calculate how much data is to be written in this block
        size_t blk_data_towrite;
				if (count < BLOCK_SIZE) {
						blk_data_towrite = count;
				} else {
						blk_data_towrite = BLOCK_SIZE;
				}
				
				// write data from buf to disk, move offset in buf
        block_write(sb->data_block + cur_db, buf);
        data_written += blk_data_towrite;
        count -= blk_data_towrite;
        buf = (void*)((char*)buf + blk_data_towrite);
    } 

		// update root directory
    void* root_buf = malloc(BLOCK_SIZE);
    block_read(sb->root_dir, root_buf);

    for (int i = 0; i < 128; i++) {
        struct root* entry = (struct root*)(root_buf + i * sizeof(struct root));

        if (strcmp(fd_table[fd].filename, entry->filename) == 0) {
            entry->size += data_written;
            if (entry->index == FAT_EOC) {
                entry->index = first_index;
            }
            break;
        }
    }

    block_write(sb->root_dir, root_buf);
    free(root_buf);

    return data_written;
}//write g

int min(int a, int b) {
  return (a < b) ? a : b;
}

int fs_read(int fd, void *buf, size_t count) {
    if (sb == NULL || fd < 0 || buf == NULL) {
        return -1;
    }

    FILE* fp = NULL;
    char* filename = NULL;
    int offset = 0;

    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (fd_table[i].fd == fd) {
            fp = fd_table[i].fp;
            filename = fd_table[i].filename;
            offset += fd_table[i].offset;
        }
    }

    if (fp == NULL || filename == NULL) {
        return -1;
    }

    void* root_dir_buffer = malloc(BLOCK_SIZE);
    block_read(sb->root_dir, root_dir_buffer);

    uint16_t file_index = FAT_EOC;
    for (int i = 0; i < 128; i++) {
        struct root* entry = (struct root*)(root_dir_buffer + i * sizeof(struct root));
        if (strcmp(entry->filename, filename) == 0) {
            file_index = entry->index;
            break;
        }
    }
    free(root_dir_buffer);
  
    if (file_index == FAT_EOC) {
        return -1;
    }
    
    void* fat_buffer = malloc(BLOCK_SIZE * sb->block_num);
    for (int i = 1; i <= sb->block_num; i++) {
        void* fat_block = malloc(BLOCK_SIZE);
        block_read(i, fat_block);
        memcpy((char*)fat_buffer + (i - 1) * BLOCK_SIZE, fat_block, BLOCK_SIZE);
        free(fat_block);
    }

    void* bounce_buffer = malloc(BLOCK_SIZE);
    size_t data_read = 0;
    size_t remaining = count;
    
    while (file_index != FAT_EOC && remaining > 0) {
        block_read(sb->data_block + file_index, bounce_buffer);
        
        size_t data_to_read = min(remaining, BLOCK_SIZE);
        memcpy((char*)buf + data_read, bounce_buffer, data_to_read);
        data_read += data_to_read;
        remaining -= data_to_read;

        memcpy(&file_index, (uint16_t*)((char*)fat_buffer + file_index * 2), sizeof(uint16_t));
    }

    free(fat_buffer);
    free(bounce_buffer);
    return data_read;
}//read
