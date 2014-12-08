#ifndef __MINIFILE_H__
#define __MINIFILE_H__

#include "defs.h"
#include "disk.h"

#define RATIO_INODE 0.1
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

disk_t *disk;

/*
 * Definitions for minifiles.
 *
 * You have to implement the fiunctions defined by this file that 
 * provide a filesystem interface for the minithread package.
 * You have to provide the implementation of these functions in
 * the file minifile.c
 */

typedef struct minifile* minifile_t;

/* Handler for disk operations */
void minifile_handle(disk_interrupt_arg_t *arg);

/* Initialize minifile */
void minifile_initialize();

/* 
 * General requiremens:
 *     If filenames and/or dirnames begin with a "/" they are absolute
 *     (the full path is specified). If they start with anything else
 *     they are relative (the full path is specified by the current 
 *     directory+filename).
 *
 *     All functions should return NULL or -1 to signal an error.
 */

/* 
 * Create a file. If the file exists its contents are truncated.
 * If the file doesn't exist it is created in the current directory.
 *
 * Returns a pointer to a structure minifile that can be used in
 *    subsequent calls to filesystem functions.
 */
minifile_t minifile_creat(char *filename);

/* 
 * Opens a file. If the file doesn't exist return NULL, otherwise
 * return a pointer to a minifile structure.
 *
 * Mode should be interpreted exactly in the manner fopen interprets
 * this argument (see fopen documentation).
 */
minifile_t minifile_open(char *filename, char *mode);

/* 
 * Reads at most maxlen bytes into buffer data from the current
 * cursor position. If there are no maxlen bytes until the end
 * of the file, it should read less (up to the end of file).
 *
 * Return: the number of bytes actually read or -1 if error.
 */
int minifile_read(minifile_t file, char *data, int maxlen);

/*
 * Writes len bytes to the current position of the cursor.
 * If necessary the file is lengthened (the new length should
 * be reflected in the inode).
 *
 * Return: 0 if everything is fine or -1 if error.
 */
int minifile_write(minifile_t file, char *data, int len);

/*
 * Closes the file. Should free the space occupied by the minifile
 * structure and propagate the changes to the file (both inode and 
 * data) to the disk.
 */
int minifile_close(minifile_t file);

/*
 * Deletes the file. The entry in the directory
 * where the file is listed is removed, the inode and the data
 * blocks are freed
 */
int minifile_unlink(char *filename);

/*
 * Creates a directory with the name dirname. 
 */
int minifile_mkdir(char *dirname);

/*
 * Removes an empty directory. Should return -1 if the directory is
 * not empty.
 */
int minifile_rmdir(char *dirname);

/* 
 * Returns information about the status of a file.
 * Returns the size of the file (possibly 0) if it is a regular file,
 * -1 if the file doesn't exist and -2 if it is a directory.
 */
int minifile_stat(char *path); 

/* Changes the current directory to path. The current directory is 
 * maintained individually for every thread and is the directory
 * to which paths not starting with "/" are relative to.
 */
int minifile_cd(char *path);

/*
 * Lists the contents of the directory path. The last pointer in the returned
 * array is set to NULL to indicate termination. This array and all its
 * constituent entries should be dynamically allocated and must be freed by 
 * the user.
 */
char **minifile_ls(char *path);

/*
 * Returns the current directory of the calling thread. This buffer should
 * be a dynamically allocated, null-terminated string that contains the current
 * directory. The caller has the responsibility to free up this buffer when done.
 */
char* minifile_pwd(void);


#endif /* __MINIFILE_H__ */
