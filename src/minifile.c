#include "minifile.h"
#include "disk.h"

#define MAGIC 4411
#define DIRECT_BLOCKS 11
#define DIRECT_PER_TABLE 1023
#define ENTRIES_PER_TABLE 15

enum { DIR_INODE = 1, FILE_INODE };

/*
 * Structure representing the superblock of a filesystem.
 * Keeps track of general information for the file system.
 */
typedef struct superblock {
    union {
        struct {
            char magic_number[4];
            char disk_size[4];

            char root_inode[4];

            char first_free_inode[4];
            char first_free_data_block[4];
        } data;

        char padding[DISK_BLOCK_SIZE];
    };
}* superblock_t;

/*
 * Structure representing an inode.
 * Acts as both directory and file inodes.
 * Keeps direct pointers and an indirect pointer.
 */
typedef struct inode {
    union {
        struct {
            char inode_type;
            char size[4];

            char direct_ptrs[DIRECT_BLOCKS][4];
            char indirect_ptr[4];
        } data;

        char padding[DISK_BLOCK_SIZE];
    };
}* inode_t;

/*
 * Structure representing an indirect block.
 * A list of direct pointers with a single indirect pointer.
 */
typedef struct indirect_block {
    union {
        struct {
            char direct_ptrs[DIRECT_PER_TABLE][4];
            char indirect_ptr[4];
        } data;

        char padding[DISK_BLOCK_SIZE];
    };
}* indirect_block_t;

/*
 * Structure representing a directory.
 * A table of names and their mappings to inode ptrs.
 */
typedef struct dir_data_block {
    union {
        struct {
            char dir_entries[ENTRIES_PER_TABLE][256];
            char inode_ptrs[ENTRIES_PER_TABLE][4];
        } data;

        char padding[DISK_BLOCK_SIZE];
    };
}* dir_data_block_t;

/*
 * Structure representing a free block. Acts like a node in a queue.
 * Keeps a reference to the next free block.
 */
typedef struct free_block {
    union {
        char next_free_block[4];
        char padding[DISK_BLOCK_SIZE];
    };
}* free_block_t;

/*
 * System wide-structure represnting a file.
 * Keeps track of all threads accessing the file.
 */
typedef struct file_data {
    /* add members here */
    int dummy;
}* file_data_t;

/*
 * struct minifile:
 *     This is the structure that keeps the information about 
 *     the opened file like the position of the cursor, etc.
 */
struct minifile {
  /* add members here */
  int dummy;
};

disk_t *disk;

minifile_t minifile_creat(char *filename) {
    return NULL;
}

minifile_t minifile_open(char *filename, char *mode) {
    return NULL;
}

int minifile_read(minifile_t file, char *data, int maxlen) {
    return 0;
}

int minifile_write(minifile_t file, char *data, int len) {
    return 0;
}

int minifile_close(minifile_t file) {
    return 0;
}

int minifile_unlink(char *filename) {
    return 0;
}

int minifile_mkdir(char *dirname) {
    return 0;
}

int minifile_rmdir(char *dirname) {
    return 0;
}

int minifile_stat(char *path) {
    return 0;
}

int minifile_cd(char *path) {
    return 0;
}

char **minifile_ls(char *path) {
    return NULL;
}

char* minifile_pwd(void) {
    return NULL;
}
