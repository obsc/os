#include "defs.h"
#include "minifile.h"
#include "minithread.h"
#include "synch.h"
#include "queue.h"
#include "disk.h"
#include "reqmap.h"
#include "miniheader.h"
#include <string.h>
#include "uthash.h"

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
    int inode_num;
    int cursor;
};

/* Threads that have the directory at blocknum open */
typedef struct dir_list {
    int blocknum; // Key
    queue_t thread_queue;
    UT_hash_handle hh;
}* dir_list_t;

// iterator function for directories
typedef int (*dir_func_t) (char*, char*, void*, void*);

disk_t *disk;
reqmap_t requests;
file_data_t *file_tbl;

dir_list_t open_dir_map;

semaphore_t metadata_lock;
superblock_t disk_superblock;

char delim[1] = "/";
char null_term[1] = "\0";
char parent[3] = "..";
char self[2] = ".";

void invalidate_dir(int blocknum) {
    dir_list_t d;
    void *f;
    thread_files_t files;

    HASH_FIND_INT( open_dir_map, &blocknum, d );
    if (!d) return;

    HASH_DEL( open_dir_map, d );

    while (queue_dequeue(d->thread_queue, &f) == 0) {
        files = (thread_files_t) f;
        files->valid = 0;
    }

    queue_free(d->thread_queue);
    free(d);
}

void move_dir(int new_blocknum) {
    dir_list_t old_dir;
    dir_list_t new_dir;
    thread_files_t files;
    int old_blocknum;

    files = minithread_directory();
    if (files->valid) { // In a valid directory
        old_blocknum = files->inode_num;
        HASH_FIND_INT( open_dir_map, &old_blocknum, old_dir );
        if (old_dir) {
            queue_delete(old_dir->thread_queue, files);
            if (queue_length(old_dir->thread_queue) == 0) { // Remove if last element
                HASH_DEL( open_dir_map, old_dir );
                queue_free(old_dir->thread_queue);
                free(old_dir);
            }
        }
    }

    files->valid = 1;
    HASH_FIND_INT( open_dir_map, &new_blocknum, new_dir );
    if (new_dir) { // Someone else already in directory
        queue_append(new_dir->thread_queue, files);
    } else { // No one else in directory
        new_dir = (dir_list_t) malloc (sizeof(struct dir_list));
        new_dir->blocknuum = new_blocknum;
        new_dir->thread_queue = queue_new();
        queue_append(new_dir->thread_queue, files);
        HASH_ADD_INT( open_dir_map, blocknum, new_dir );
    }
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

void write_block_blocking(int block_num, char* block) {
    waiting_request_t req;

    req = write_block(block_num, block);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        free(req);
        return;
    }
    free(req);
}

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
    //int inode_num;
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
            //inode_num = unpack_unsigned_int(cur_block->data.inode_ptrs[acc]);
            if (f(cur_block->data.dir_entries[acc], cur_block->data.inode_ptrs[acc], arg, result) == 0) {
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
    //int inode_num;
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
            //inode_num = unpack_unsigned_int(cur_block->data.inode_ptrs[acc]);
            if (f(cur_block->data.dir_entries[acc], cur_block->data.inode_ptrs[acc], arg, result) == 0) {
                free(cur_block);
                return 0;
            }
        }
    }
    free(cur_block);
    indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(dir->data.indirect_ptr));

    return dir_iterate_indir(indir, f, arg, result, size);
}

int get_inode_helper(char *item, char *inode_num, void *arg, void *result) {
    int *res;
    int actual_num;

    actual_num = unpack_unsigned_int(inode_num);

    if ((strcmp(item, (char *) arg) == 0) && actual_num != 0) {
        res = (int *) result;
        *res = actual_num;
        return 0;
    }
    return -1;
}

inode_t get_inode(char *path, int *inode_num) {
    inode_t current;
    int found;
    char *token;
    thread_files_t cur_thread;
    char *saveptr;

    if (!path) return NULL;
    if (strlen(path) == 0) return NULL;

    if (path[0] == '/') {
        current = (inode_t) get_block_blocking(minifile_get_root_num());
        found = 1;
    } else {
        cur_thread = minithread_directory();
        if (!cur_thread || cur_thread->valid == 0) return NULL;
        current = (inode_t) get_block_blocking(cur_thread->inode_num);
    }

    token = strtok_r(path, "/", &saveptr);

    while (token != NULL) {
        if (dir_iterate(current, get_inode_helper, token, &found) == -1) {
            free(current);
            return NULL;
        }
        free(current);

        current = (inode_t) get_block_blocking(found);
        token = strtok_r(NULL, "/", &saveptr);
    }

    *inode_num = found;
    return current;
}

char* get_free_inode_block(int *block_num) {
    free_block_t freeblock;
    int nextblock;

    *block_num = unpack_unsigned_int(disk_superblock->data.first_free_inode);

    if (*block_num == 0) {
        return NULL;
    } else {
        freeblock = (free_block_t) get_block_blocking(*block_num);
        nextblock = unpack_unsigned_int(freeblock->next_free_block);
        pack_unsigned_int(disk_superblock->data.first_free_inode, nextblock);
        write_block_blocking(0, (char *) disk_superblock);
    }
    return (char *) freeblock;
}

char* get_free_data_block(int *block_num) {
    free_block_t freeblock;
    int nextblock;

    *block_num = unpack_unsigned_int(disk_superblock->data.first_free_data_block);

    if (*block_num == 0) {
        return NULL;
    } else {
        freeblock = (free_block_t) get_block_blocking(*block_num);
        nextblock = unpack_unsigned_int(freeblock->next_free_block);
        pack_unsigned_int(disk_superblock->data.first_free_data_block, nextblock);
        write_block_blocking(0, (char *) disk_superblock);
    }
    return (char *) freeblock;
}

void set_free_inode_block(int block_num, char *block) {
    free_block_t freeblock;
    int first_free;

    if (block_num == 0 || block == NULL) {
        return;
    }
    freeblock = (free_block_t) block;
    first_free = unpack_unsigned_int(disk_superblock->data.first_free_inode);
    pack_unsigned_int(freeblock->next_free_block, first_free);
    pack_unsigned_int(disk_superblock->data.first_free_inode, block_num);

    write_block_blocking(block_num, block);
    write_block_blocking(0, (char *) disk_superblock);
}

void set_free_data_block(int block_num, char *block) {
    free_block_t freeblock;
    int first_free;

    if (block_num == 0 || block == NULL) {
        return;
    }
    freeblock = (free_block_t) block;
    first_free = unpack_unsigned_int(disk_superblock->data.first_free_data_block);
    pack_unsigned_int(freeblock->next_free_block, first_free);
    pack_unsigned_int(disk_superblock->data.first_free_data_block, block_num);

    write_block_blocking(block_num, block);
    write_block_blocking(0, (char *) disk_superblock);
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

/* Initialize minifile globals */
void minifile_initialize() {
    file_tbl = (file_data_t *) calloc (disk_size, sizeof(file_data_t));

    if (!file_tbl) return;

    requests = reqmap_new(MAX_PENDING_DISK_REQUESTS);
    if (!requests) {
        free(file_tbl);
        return;
    }

    metadata_lock = semaphore_create();
    semaphore_initialize(metadata_lock, 1);
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

int write_new_dir(int prevdir, dir_data_block_t dir, int dir_num, int entry_num, char *name) {
    inode_t newdir;
    dir_data_block_t newdir_data;
    int newdir_num;
    int newdir_data_num;
    int i;

    newdir = (inode_t) get_free_inode_block(&newdir_num);
    newdir_data = (dir_data_block_t) get_free_data_block(&newdir_data_num);

    if (!newdir || !newdir_data) {
        set_free_inode_block(newdir_num, (char *) newdir);
        set_free_data_block(newdir_data_num, (char *) newdir_data);
        free(newdir);
        free(newdir_data);
        return -1;
    }

    pack_unsigned_int(newdir_data->data.inode_ptrs[0], prevdir);
    memcpy(newdir_data->data.dir_entries[0], parent, 3);
    pack_unsigned_int(newdir_data->data.inode_ptrs[1], newdir_num);
    memcpy(newdir_data->data.dir_entries[1], self, 2);
    for (i = 2; i < ENTRIES_PER_TABLE; i++) {
        pack_unsigned_int(newdir_data->data.inode_ptrs[i], 0);
    }

    newdir->data.inode_type = DIR_INODE;
    pack_unsigned_int(newdir->data.size, 2);
    pack_unsigned_int(newdir->data.direct_ptrs[0], newdir_data_num);
    for (i = 1; i < DIRECT_BLOCKS; i++) {
        pack_unsigned_int(newdir->data.direct_ptrs[i], 0);
    }
    pack_unsigned_int(newdir->data.indirect_ptr, 0);

    strcpy(dir->data.dir_entries[entry_num], name);
    pack_unsigned_int(dir->data.inode_ptrs[entry_num], newdir_num);

    write_block_blocking(newdir_data_num, (char *) newdir_data);
    write_block_blocking(newdir_num, (char *) newdir);
    write_block_blocking(dir_num, (char *) dir);

    free(newdir);
    free(newdir_data);
    return 0;
}

int mkdir_helper(inode_t dir, int inode_num, char *name) {
    int direct_ind;
    dir_data_block_t direct_block;
    int direct_block_num;
    int entry_num;
    int size;
    indirect_block_t indir_block;
    int indir_block_num;
    indirect_block_t next_indir_block;
    int next_indir_block_num;

    size = unpack_unsigned_int(dir->data.size);
    pack_unsigned_int(dir->data.size, size + 1);
    if (size < DIRECT_BLOCKS * ENTRIES_PER_TABLE) { // found in a direct block
    direct_ind = size / ENTRIES_PER_TABLE;
        entry_num = size % ENTRIES_PER_TABLE;
        direct_block_num = unpack_unsigned_int(dir->data.direct_ptrs[direct_ind]);
        if (entry_num == 0) { // New direct block
            direct_block = (dir_data_block_t) get_free_data_block(&direct_block_num);
            if (!direct_block) {
                free(dir);
                return -1;
            }
            pack_unsigned_int(dir->data.direct_ptrs[direct_ind], direct_block_num);
        } else if (direct_block_num == 0) {
            free(dir);
            return -1;
        } else {
            direct_block = (dir_data_block_t) get_block_blocking(direct_block_num);
        }
        if (write_new_dir(inode_num, direct_block, direct_block_num, entry_num, name) == -1) {
            if (entry_num == 0) {
                set_free_data_block(direct_block_num, (char *) direct_block);
            }
            free(direct_block);
            free(dir);
            return -1;
        }
        write_block_blocking(inode_num, (char *) dir);
        free(dir);
        return 0;
    }

    size -= DIRECT_BLOCKS * ENTRIES_PER_TABLE; // Remaining size
    if (size == 0) { // first entry in indirect
        indir_block = (indirect_block_t) get_free_data_block(&indir_block_num);
        if (!indir_block) {
            free(dir);
            return -1;
        }
        pack_unsigned_int(dir->data.indirect_ptr, indir_block_num);
    } else {
        indir_block_num = unpack_unsigned_int(dir->data.indirect_ptr);
        indir_block = (indirect_block_t) get_block_blocking(indir_block_num);
    }

    while (size >= DIRECT_PER_TABLE * ENTRIES_PER_TABLE) {
        size -= DIRECT_PER_TABLE * ENTRIES_PER_TABLE;
        if (size == 0) { // first entry in indirect
            next_indir_block = (indirect_block_t) get_free_data_block(&next_indir_block_num);
            if (!next_indir_block) {
                free(dir);
                return -1;
            }
            pack_unsigned_int(indir_block->data.indirect_ptr, next_indir_block_num);
            write_block_blocking(indir_block_num, (char *) indir_block);
        } else {
            next_indir_block_num = unpack_unsigned_int(indir_block->data.indirect_ptr);
            next_indir_block = (indirect_block_t) get_block_blocking(next_indir_block_num);
        }

        free(indir_block);
        indir_block = next_indir_block;
        indir_block_num = next_indir_block_num;
    }

    direct_ind = size / ENTRIES_PER_TABLE;
    entry_num = size % ENTRIES_PER_TABLE;
    direct_block_num = unpack_unsigned_int(indir_block->data.direct_ptrs[direct_ind]);

    if (entry_num == 0) { // New direct block
        direct_block = (dir_data_block_t) get_free_data_block(&direct_block_num);
        if (!direct_block) {
            if (size == 0) {
                set_free_data_block(indir_block_num, (char *) indir_block);
            }
            free(indir_block);
            free(dir);
            return -1;
        }
        pack_unsigned_int(indir_block->data.direct_ptrs[direct_ind], direct_block_num);
    } else if (direct_block_num == 0) {
        if (size == 0) {
            set_free_data_block(indir_block_num, (char *) indir_block);
        }
        free(indir_block);
        free(dir);
        return -1;
    } else {
        direct_block = (dir_data_block_t) get_block_blocking(direct_block_num);
    }
    if (write_new_dir(inode_num, direct_block, direct_block_num, entry_num, name) == -1) {
        if (entry_num == 0) {
            set_free_data_block(direct_block_num, (char *) direct_block);
        }
        if (size == 0) {
            set_free_data_block(indir_block_num, (char *) indir_block);
        }
        free(direct_block);
        free(dir);
        return -1;
    }

    write_block_blocking(indir_block_num, (char *) indir_block);
    write_block_blocking(inode_num, (char *) dir);
    free(dir);
    return 0;
}

int minifile_mkdir(char *dirname) {
    thread_files_t files;
    char *dir;
    char *name;
    inode_t prevdir;
    int prevdir_num;
    int dirlen;
    char *dircopy;
    int test_num;
    inode_t test_inode;

    dirlen = strlen(dirname);
    dircopy = (char *) malloc (dirlen+1);
    memcpy(dircopy, dirname, dirlen+1);

    test_inode = get_inode(dircopy, &test_num);
    if (test_inode) { // Directory already exists
        free(test_inode);
        free(dircopy);
        return -1;
    }

    free(test_inode);
    free(dircopy);

    name = strrchr(dirname, '/');

    if (!name) { // Current directory
    name = dirname;
        files = minithread_directory();
        if (!files) return -1;
        prevdir = (inode_t) get_block_blocking(files->inode_num);
        prevdir_num = files->inode_num;
    } else {
    name++;
        dir = (char *) malloc (name - dirname + 1);
        memcpy(dir, dirname, name - dirname);
        memcpy(dir + (name - dirname), null_term, 1);
        prevdir = get_inode(dir, &prevdir_num);
        free(dir);
    }

    if (strlen(name) == 0 || strlen(name) > 256) return -1;
    if (!prevdir || prevdir->data.inode_type == FILE_INODE) {
        free(prevdir);
        return -1;
    }

    return mkdir_helper(prevdir, prevdir_num, name);
}


int rm_last(inode_t dir, int dir_num, char **result_num, char **result) {
    unsigned int blockid;
    dir_data_block_t cur_block;
    int size;
    indirect_block_t indir;
    int indir_num;
    indirect_block_t temp;
    int temp_num;

    size = unpack_unsigned_int(dir->data.size);
    if (((size - 1) / ENTRIES_PER_TABLE) < DIRECT_BLOCKS) {
        blockid = unpack_unsigned_int(dir->data.direct_ptrs[(size -1) / ENTRIES_PER_TABLE]);
        if (blockid == 0) {
            return -1;
        }
        cur_block = (dir_data_block_t) get_block_blocking(blockid);
        *result_num = (char *) malloc (1 + strlen(cur_block->data.inode_ptrs[(size -1) % ENTRIES_PER_TABLE]));
        if (!(*result_num)) {
            free(cur_block);
            return -1;
        }
        memcpy(*result_num, cur_block->data.inode_ptrs[(size -1) % ENTRIES_PER_TABLE], strlen(cur_block->data.inode_ptrs[(size -1) % ENTRIES_PER_TABLE]) + 1);
        *result = (char *) malloc (1 + strlen(cur_block->data.dir_entries[(size - 1) % ENTRIES_PER_TABLE]));
        if (!(*result)) {
            free(*result_num);
            free(cur_block);
            return -1;
        }
        memcpy(*result, cur_block->data.dir_entries[(size -1) % ENTRIES_PER_TABLE], 1 + strlen(cur_block->data.dir_entries[(size - 1) % ENTRIES_PER_TABLE]));
        if (((size - 1) % ENTRIES_PER_TABLE) == 0) {
            set_free_data_block(blockid, (char *)cur_block);
            pack_unsigned_int(dir->data.direct_ptrs[(size -1) / ENTRIES_PER_TABLE], 0);
            write_block_blocking(dir_num, (char *)dir);
        }
        free(cur_block);
    } else {
        size = size - (ENTRIES_PER_TABLE * DIRECT_BLOCKS);
        temp = NULL;
        temp_num = -1;
        indir_num = unpack_unsigned_int(dir->data.indirect_ptr);
        indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(dir->data.indirect_ptr));
        while (size > (DIRECT_PER_TABLE * ENTRIES_PER_TABLE)) {
            size = size - (DIRECT_PER_TABLE * ENTRIES_PER_TABLE); 
            temp = indir;
            temp_num = indir_num;
            indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(temp->data.indirect_ptr));
            indir_num = unpack_unsigned_int(temp->data.indirect_ptr);
        }

        if (((size - 1) / ENTRIES_PER_TABLE) < DIRECT_PER_TABLE) {
            blockid = unpack_unsigned_int(indir->data.direct_ptrs[(size -1) / ENTRIES_PER_TABLE]);
            if (blockid == 0) {
                free(indir);
                free(temp);
                return -1;
            }
            cur_block = (dir_data_block_t) get_block_blocking(blockid);
            *result_num = (char *) malloc (1 + strlen(cur_block->data.inode_ptrs[(size -1) % ENTRIES_PER_TABLE]));
            if (!(*result_num)) {
                free(cur_block);
                free(indir);
                free(temp);
                return -1;
            }
            memcpy(*result_num, cur_block->data.inode_ptrs[(size -1) % ENTRIES_PER_TABLE], strlen(cur_block->data.inode_ptrs[(size -1) % ENTRIES_PER_TABLE]) + 1);
            *result = (char *) malloc (1 + strlen(cur_block->data.dir_entries[(size - 1) % ENTRIES_PER_TABLE]));
            if (!(*result)) {
                free(*result_num);
                free(cur_block);
                free(indir);
                free(temp);
                return -1;
            }
            memcpy(*result, cur_block->data.dir_entries[(size -1) % ENTRIES_PER_TABLE], 1 + strlen(cur_block->data.dir_entries[(size - 1) % ENTRIES_PER_TABLE]));

            if (((size - 1) % ENTRIES_PER_TABLE) == 0) {
                set_free_data_block(blockid, (char *)cur_block);
                pack_unsigned_int(indir->data.direct_ptrs[(size -1) / ENTRIES_PER_TABLE], 0);
                write_block_blocking(indir_num, (char *)indir);
            }
            free(cur_block);

            if (size == 1) {
                set_free_data_block(indir_num, (char *)indir);
                if (!temp) {
                    pack_unsigned_int(dir->data.indirect_ptr, 0);
                    write_block_blocking(dir_num, (char *)dir);
                } else {
                    pack_unsigned_int(temp->data.indirect_ptr, 0);
                    write_block_blocking(temp_num, (char *)temp);
                }
            }
            free(indir);
            free(temp);
        }

    }

    pack_unsigned_int(dir->data.size, unpack_unsigned_int(dir->data.size)-1);
    write_block_blocking(dir_num, (char *)dir);
    return 0;
}

typedef struct num_struct {
    char *num;
    char *structure;
}* num_struct_t;

int remove_inode_helper(char *item, char *inode_num, void *arg, void *result) {
    char *number;
    int *num;
    char *name;
    int actual_num;
    num_struct_t nstruct;

    nstruct = (num_struct_t) result;

    name = nstruct->structure;
    number = nstruct->num;

    actual_num = unpack_unsigned_int(inode_num);
    num = (int *)arg;

    if (*num == actual_num) {
        memcpy(item, name, strlen(name)+1);
        memcpy(inode_num, number, strlen(number)+1);
        return 0;
    }
    return -1;
}

int remove_inode(int dir_num, int item_num) {
    inode_t dir;
    char *result_num;
    char *result;
    num_struct_t nstruct;

    dir = (inode_t) get_block_blocking(dir_num);

    if (rm_last(dir, dir_num, &result_num, &result) == 0) {
        nstruct = (num_struct_t) malloc (sizeof(struct num_struct));
        nstruct->num = result_num;
        nstruct->structure = result;
        if (dir_iterate(dir, remove_inode_helper, &item_num, nstruct) == 0) {
            free(result);
            free(result_num);
            free(nstruct);
            return 0;
        }
    } else {
        return -1;
    }
    return -1;
}

int minifile_rmdir(char *dirname) {
    inode_t block;
    int blockid;
    int size;
    int parent_num;
    dir_data_block_t freed_data;

    if ((strcmp(dirname, "..") == 0) || (strcmp(dirname, ".") == 0)) {
        return -1;
    }
    block = get_inode(dirname, &blockid);

    if (!block) return -1;
    size = unpack_unsigned_int(block->data.size);
    if (block->data.inode_type == FILE_INODE || size > 2) {
        free(block);
        return -1;
    }

    freed_data = (dir_data_block_t) get_block_blocking(unpack_unsigned_int(block->data.direct_ptrs[0]));

    parent_num = unpack_unsigned_int(freed_data->data.inode_ptrs[0]);

    set_free_data_block(unpack_unsigned_int(block->data.direct_ptrs[0]), (char *)freed_data);
    set_free_inode_block(blockid, (char *)block);

    if (remove_inode(parent_num, blockid) == -1) return -1;

    invalidate_dir(blockid);

    return 0;
}

int minifile_stat(char *path) {
    inode_t block;
    int dummy;
    int size;

    block = get_inode(path, &dummy);
    if (!block) return -1;
    if (block->data.inode_type == FILE_INODE) {
        size = unpack_unsigned_int(block->data.size);
        free(block);
        return size;
    } else {
        free(block);
        return -2;
    }
}

int minifile_cd(char *path) {
    thread_files_t files;
    inode_t block;
    int inode_num;
    char *token;
    int acc;
    str_and_len_t result;
    void *data;
    int temp;
    char *path_copy;
    int path_len;
    char *saveptr;

    path_len = strlen(path);
    path_copy = (char *) malloc (path_len+1);
    memcpy(path_copy, path, path_len+1);
    block = get_inode(path, &inode_num);
    if (!block || block->data.inode_type == FILE_INODE) {
        free(block);
        return -1;
    }
    free(block);
    files = minithread_directory();
    if (path_copy[0] == '/') {
        files->path_len = 2;
        temp = queue_length(files->path);
        for (acc = 0; acc < temp; acc++) {
            if (queue_dequeue(files->path, &data) == 0) {
                result = (str_and_len_t) data;
                free(result);
            } else {
                free(path_copy);
                return -1;
            }
        }
    }

    token = strtok_r(path_copy, "/", &saveptr);

    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            if (queue_dequeue(files->path, &data) == 0) {
                result = (str_and_len_t) data;
                if (queue_length(files->path) == 0) {
                    files->path_len -= result->len;
                } else {
                    files->path_len -= result->len + 1;
                }
                free(result->data);
                free(result);
            }
        } else if (strcmp(token, ".") != 0) {
            result = (str_and_len_t) malloc (sizeof(struct str_and_len));
            result->data = (char *) malloc (strlen(token));
            memcpy(result->data, token, strlen(token));
            result->len = strlen(token);
            if (queue_length(files->path) == 0) {
                files->path_len += strlen(token);
            } else {
                files->path_len += strlen(token) + 1;
            }
            if (queue_prepend(files->path, result) == -1) {
                free(path_copy);
                return -1;
            }
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    move_dir(files->inode_num, inode_num);
    files->inode_num = inode_num;

    free(path_copy);
    return 0;
}

int ls_helper(char *item, char *inode_num, void *arg, void *result) {
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
        if (!files || files->valid == 0) return NULL;
        block = (inode_t) get_block_blocking(files->inode_num);
    } else {
        block = get_inode(path, &dummy);
    }

    if (!block || block->data.inode_type == FILE_INODE) {
        free(block);
        return NULL;
    }
    else {
        size = unpack_unsigned_int(block->data.size);
        file_list = (char **) malloc (sizeof(char *) * (size + 1));
        ptr = file_list;
        dir_iterate(block, ls_helper, NULL, &ptr);
        file_list[size] = NULL;
    }

    free(block);
    return file_list;
}

void rev_add_to_path(void *item, void *ptr) {
    char **path_ptr;
    str_and_len_t dir;

    path_ptr = (char **) ptr;
    dir = (str_and_len_t) item;

    *path_ptr -= dir->len + 1;
    memcpy(*path_ptr, delim, 1);
    memcpy(*path_ptr + 1, dir->data, dir->len);
}

char* minifile_pwd(void) {
    thread_files_t files;
    char *path;
    char *ptr;

    files = minithread_directory();
    if (!files) return NULL;

    path = (char *) malloc (files->path_len);
    memcpy(path, delim, 1);
    ptr = path + files->path_len - 1;
    memcpy(ptr, null_term, 1);

    queue_iterate(files->path, rev_add_to_path, &ptr);

    return path;
}
