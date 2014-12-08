#include "defs.h"
#include "minifile.h"
#include "synch.h"
#include "disk.h"

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
 * System wide-structure represnting a file.
 * Keeps track of all threads accessing the file.
 */
typedef struct file_data {
    inode_t file_inode;
    int num;
    semaphore_t close_wait;
}* file_data_t;

/*
 * struct minifile:
 *     This is the structure that keeps the information about 
 *     the opened file like the position of the cursor, etc.
 */
struct minifile {
    int inodeNum;
    int cursor;
};

typedef struct waiting_request {
    semaphore_t wait;
}* waiting_request_t;

disk_t *disk;
file_data_t *file_tbl;

// typedef struct {
//   disk_t* disk;
//   disk_request_t request;
//   disk_reply_t reply;
// } disk_interrupt_arg_t;

// typedef struct {
//   int blocknum;
//   char* buffer; /* pointer to the memory buffer */
//   disk_request_type_t type; /* type of disk request */
// } disk_request_t;

/* Handler for disk operations
 * Assumes interrupts are disabled within
 */
void minifile_handle(disk_interrupt_arg_t *arg) {
    free(arg);
}

void minifile_initialize() {
    file_tbl = (file_data_t *) calloc (disk_size, sizeof(file_data_t));
}

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
