#include "defs.h"
#include "minifile.h"
#include "synch.h"
#include "queue.h"
#include "disk.h"
#include "reqmap.h"
#include "miniheader.h"
#include <string.h>

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

superblock_t disk_superblock;
inode_t disk_root;

inode_t get_inode(char *path) {
	//inode_t current;
	char *token;

	if (!path) return NULL;
	if (strlen(path) == 0) return NULL; 

	if (path[0] == '/') {
		//current = root
	} else {
		//current = current
	}

	token = strtok(path, "/");
    return NULL;
}

/* Handler for disk operations
 * Assumes interrupts are disabled within
 */
void minifile_handle(disk_interrupt_arg_t *arg) {
    int blockid;
    char* buffer;
    waiting_request_t req;
    void* req_addr;

    blockid = arg->request.blocknum;
    buffer = arg->request.buffer;
    reqmap_get(requests, blockid, buffer, &req_addr);
    req = (waiting_request_t) req_addr;

    req->reply = arg->reply;
    semaphore_V(req->wait);

    reqmap_delete(requests, blockid, buffer);

    free(arg);
}

waiting_request_t createWaiting(int blockid, char* buffer) {
    waiting_request_t req;

    req = (waiting_request_t) malloc (sizeof (struct waiting_request));
    if (!req) return NULL;

    req->wait = semaphore_create();
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

/* Initialize minifile globals
 */
void minifile_initialize() {
    waiting_request_t req;
    int magic_num;
    int root_num;

    file_tbl = (file_data_t *) calloc (disk_size, sizeof(file_data_t));

    if (!file_tbl) return;

    requests = reqmap_new(MAX_PENDING_DISK_REQUESTS);
    if (!requests) {
        free(file_tbl);
        return;
    }

    req = read_block(0, (char *) disk_superblock);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to load superblock\n");
        free(req);
        return;
    }
    free(req);

    magic_num = unpack_unsigned_int(disk_superblock->data.magic_number);
    if (magic_num != MAGIC) {
        printf("Invalid magic number\n");
        return;
    }
    root_num = unpack_unsigned_int(disk_superblock->data.root_inode);

    req = read_block(root_num, (char *) disk_root);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to load root\n");
        free(req);
        return;
    }
    free(req);
}

inode_t minifile_get_root() {
    return disk_root;
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
