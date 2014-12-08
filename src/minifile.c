#include "defs.h"
#include "minifile.h"
#include "synch.h"
#include "disk.h"
#include "reqmap.h"

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

disk_t *disk;
reqmap_t requests;
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
    int blockid;
    char* buffer;
    waiting_request_t req;
    void* req_addr;

    blockid = arg->request.blockid;
    buffer = arg->request.buffer;
    reqmap_get(requests, blockid, buffer, &req_addr);
    req = (waiting_request_t) req_addr;

    req->reply = arg->reply;
    semaphore_V(req->wait);

    reqmap_delete(requests, blockid, buffer);

    free(arg);
}

/* Initialize minifile globals
 */
void minifile_initialize() {
    file_tbl = (file_data_t *) calloc (disk_size, sizeof(file_data_t));

    if (!file_tbl) return;

    requests = reqmap_new(MAX_PENDING_DISK_REQUESTS);
    if (!requests) {
        free(file_tbl);
    }
}

waiting_request_t createWaiting(int blockid, char* buffer) {
    waiting_request_t req;

    req = (waiting_request_t) malloc (sizeof (struct waiting_request));
    if (!req) return NULL;

    req->wait = semaphore_new();
    if (!req->wait) {
        free(req);
        return NULL;
    }
    semaphore_initialize(req->wait, 0);

    reqmap_set(requests, blockid, buffer, req);
    return req;
}

waiting_request_t read_block(int blockid, char* buffer) {
    waiting_request_t req = createWaiting(blockid, buffer);
    if (!req) return NULL;
    disk_read_block(disk, blockid, buffer);
    return req;
}

waiting_request_t write_block(int blockid, char* buffer) {
    waiting_request_t req = createWaiting(blockid, buffer);
    if (!req) return NULL;
    disk_write_block(disk, blockid, buffer);
    return req;
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
