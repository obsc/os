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

// iterator function for directories
typedef int (*dir_func_t) (char*, int, void*, void*);

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

int dir_iterate_indir(indirect_block_t indir, dir_func_t f, void* arg, void* result, int cur_size) {
    unsigned int blockid;
    dir_data_block_t cur_block;
    int acc;
    int size;
    int inode_num;
    int num_to_check;
    indirect_block_t indirect;

    if (cur_size == 0) {
        free(indir);
        return -1;
    }

    size = cur_size;
    for (acc = 0; acc < DIRECT_PER_TABLE; acc++) {
        blockid = unpack_unsigned_int(indir->data.direct_ptrs[acc]);
        if (blockid == 0) {
            free(cur_block);
            free(indir);
            return -1;
        }
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
            if (f(cur_block->data.dir_entries[acc], inode_num, arg, result) == 0) {
                free(cur_block);
                free(indir);
                return 0;
            }
        }
    }
    free(cur_block);
    indirect = (indirect_block_t) get_block_blocking(unpack_unsigned_int(indir->data.indirect_ptr));
    free(indir);

    return dir_iterate_indir(indirect, f, arg, result, size);
}

int dir_iterate(inode_t dir, dir_func_t f, void* arg, void* result) {
    unsigned int blockid;
    dir_data_block_t cur_block;
    int acc;
    int size;
    int inode_num;
    int num_to_check;
    indirect_block_t indir;

    size = unpack_unsigned_int(dir->data.size);

    for (acc = 0; acc < DIRECT_BLOCKS; acc++) {
        blockid = unpack_unsigned_int(dir->data.direct_ptrs[acc]);
        if (blockid == 0) {
            free(cur_block);
            return -1;
        }
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
            if (f(cur_block->data.dir_entries[acc], inode_num, arg, result) == 0) {
                free(cur_block);
                return 0;
            }
        }
    }
    free(cur_block);
    indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(dir->data.indirect_ptr));

    return dir_iterate_indir(indir, f, arg, result, size);
}

int get_inode_helper(char *item, int inode_num, void *arg, void *result) {
    int *res;

    if ((strcmp(item, (char *) arg) == 0) && inode_num != 0) {
        res = (int *) result;
        *res = inode_num;
        return 0;
    }
    return -1;
}

inode_t get_inode(char *path, int *inode_num) {
    inode_t current;
    int found;
    char *token;
    thread_files_t cur_thread;

    if (!path) return NULL;
    if (strlen(path) == 0) return NULL;


    if (path[0] == '/') {
        current = (inode_t) get_block_blocking(minifile_get_root_num());
    } else {
        cur_thread = minithread_directory();
        if (!cur_thread) {
            return NULL;
        }
        current = (inode_t) get_block_blocking(cur_thread->inode_num);
    }

    token = strtok(path, "/");

    while (token != NULL) {
        if (dir_iterate(current, get_inode_helper, token, &found) == -1) {
            free(current);
            return NULL;
        }
        free(current);

        current = (inode_t) get_block_blocking(found);
        token = strtok(NULL, "/");
    }

    *inode_num = found;
    return current;
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
    inode_t block;
    int dummy;

    block = get_inode(path, &dummy);
    if (!block) return -1;
    if (block->data.inode_type == FILE_INODE) {
        return unpack_unsigned_int(block->data.size);
    } else return -2;
}

int minifile_cd(char *path) {
    thread_files_t files;
    inode_t block;
    int inode_num;

    block = get_inode(path, &inode_num);
    if (!block || block->data.inode_type == FILE_INODE) return -1;
    else {
        files = minithread_directory();
        files->inode_num = inode_num;
    }

    return 0;
}

int ls_helper(char *item, int inode_num, void *arg, void *result) {
    char ***file_list_ptr;
    int len;

    file_list_ptr = (char ***) result;
    len = strlen(item);
    **file_list_ptr = (char *) malloc (len + 1);
    memcpy(**file_list_ptr, item, len + 1);

    *file_list_ptr += 1;

    return -1;
}

char **minifile_ls(char *path) {
    thread_files_t files;
    inode_t block;
    int size;
    char **file_list;
    char **ptr;
    int dummy;

    if (!path || strlen(path) == 0) {
        files = minithread_directory();
        if (!files) return NULL;
        block = (inode_t) get_block_blocking(files->inode_num);
    } else {
        block = get_inode(path, &dummy);
    }

    if (!block || block->data.inode_type == FILE_INODE) return NULL;
    else {
        size = unpack_unsigned_int(block->data.size);
        file_list = (char **) malloc (sizeof(char *) * (size + 1));
        ptr = file_list;
        dir_iterate(block, ls_helper, NULL, &ptr);
        file_list[size] = NULL;
    }

    return file_list;
}

void add_to_path(void *item, void *ptr) {
    char **path_ptr;
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
    char *path;
    char *ptr;

    files = minithread_directory();
    if (!files) return NULL;

    path = (char *) malloc (files->path_len);
    memcpy(path, delim, 1);
    memcpy(path + 1, null_term, 1);
    ptr = path;

    queue_iterate(files->path, add_to_path, &ptr);

    return path;
}
