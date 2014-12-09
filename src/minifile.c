#include "defs.h"
#include "minifile.h"
#include "minithread.h"
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

char delim[1] = "/";
char null_term[1] = "\0";

char* get_block_blocking(int block_num) {
    waiting_request_t req;
    char* block;

    block = (char *) malloc (DISK_BLOCK_SIZE);

    req = read_block(block_num, block);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to load block: %i\n", block_num);
        free(req);
        free(block);
        return NULL;
    }

    free(req);
    return block;
}

inode_t get_inode_indir() {
    return NULL;
}

inode_t get_inode_helper(char *path, inode_t inode) {
	unsigned int blockid;
	dir_data_block_t cur_block;
	int acc;
	int size;
	int inode_num;
	int num_to_check;

    cur_block = (dir_data_block_t) malloc (sizeof(struct dir_data_block));
	size = unpack_unsigned_int(inode->data.size);

	for (acc = 0; acc < DIRECT_BLOCKS; acc++) {
		blockid = unpack_unsigned_int(inode->data.direct_ptrs[acc]);
		if (blockid == 0) return NULL;
		cur_block = (dir_data_block_t) get_block_blocking(blockid);

		if (size > ENTRIES_PER_TABLE) {
			num_to_check = ENTRIES_PER_TABLE;
			size = size - ENTRIES_PER_TABLE;
		} else {
			num_to_check = size;
			size = 0;
		}
		for (acc = 0; acc < num_to_check; acc++) {
			inode_num = unpack_unsigned_int(cur_block->data.inode_ptrs[acc]);
			if ((strcmp(cur_block->data.dir_entries[acc], path) == 0) 
				&& inode_num != 0) {
				return (inode_t) get_block_blocking(inode_num);
			}
		}
	}


    return NULL;
}

inode_t get_inode(char *path) {
	//inode_t current;
    //inode_t found;
    char *token;

	if (!path) return NULL;
	if (strlen(path) == 0) return NULL; 

	if (path[0] == '/') {
		//current = root
	} else {
		//current = current
	}

	token = strtok(path, "/");

	while (token != NULL) {

	}
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

/* Initialize minifile globals */
void minifile_initialize() {
    file_tbl = (file_data_t *) calloc (disk_size, sizeof(file_data_t));

    if (!file_tbl) return;

    requests = reqmap_new(MAX_PENDING_DISK_REQUESTS);
    if (!requests) {
        free(file_tbl);
        return;
    }

    disk_superblock = (superblock_t) malloc (sizeof(struct superblock));
}

/* Initialize minifile global blocks */
void minifile_initialize_blocks() {
    waiting_request_t req;
    int magic_num;

    if (!use_existing_disk) return;
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
}

int minifile_get_root_num() {
    return unpack_unsigned_int(disk_superblock->data.root_inode);
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

void add_to_path(void *item, void *ptr) {
    char** path_ptr;
    str_and_len_t dir;

    path_ptr = (char **) ptr;
    dir = (str_and_len_t) item;
    memcpy(*path_ptr, delim, 1);
    memcpy(*path_ptr + 1, dir->data, dir->len);

    *path_ptr += dir->len + 1;
    memcpy(*path_ptr, null_term, 1);
 }

char* minifile_pwd(void) {
    thread_files_t files;
    char* path;
    char* ptr;

    files = minithread_directory();
    if (!files) return NULL;

    path = (char *) malloc (files->path_len);
    memcpy(path, delim, 1);
    memcpy(path + 1, null_term, 1);
    ptr = path;

    queue_iterate(files->path, add_to_path, &ptr);

    return path;
}
