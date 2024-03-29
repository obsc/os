#include "defs.h"
#include "minifile.h"
#include "minithread.h"
#include "synch.h"
#include "queue.h"
#include "disk.h"
#include "miniheader.h"
#include <string.h>
#include "uthash.h"
#include "counter.h"

/* Structure for locking access to an inode
 */
typedef struct inode_lock {
    int blocknum; // key
    counter_t mutex;
    UT_hash_handle hh;
}* inode_lock_t;

/*
 * System wide-structure represnting a file.
 */
typedef struct file_access {
    int blocknum; // key
    char unlinked;
    int num;
    UT_hash_handle hh;
}* file_access_t;

/*
 * struct minifile:
 *     This is the structure that keeps the information about 
 *     the opened file like the position of the cursor, etc.
 */
struct minifile {
    int inode_num;
    int cursor;
    char readable;
    char writeable;
    char append;
};

/* Threads that have the directory at blocknum open */
typedef struct dir_list {
    int blocknum; // key
    queue_t thread_queue;
    UT_hash_handle hh;
}* dir_list_t;

/*
 * Structure representing a waiting disk request
 */
typedef struct waiting_request {
    int blocknum; // key
    disk_reply_t *reply;
    semaphore_t wait_for_reply;
    counter_t mutex;
    UT_hash_handle hh;
}* waiting_request_t;

/*
 * Strucutre denoting a blockid and block
 */
typedef struct num_struct {
    char *num;
    char *structure;
}* num_struct_t;

// iterator function for directories
typedef int (*dir_func_t) (char*, char*, void*, void*, int, char*);
// iterator function for files
typedef int (*file_func_t) (char*, void*, void*);
// Function for constructing inodes
typedef int (*write_func_t) (inode_t, int, int);

disk_t *disk;

waiting_request_t req_map;
file_access_t file_map;
dir_list_t open_dir_map;
inode_lock_t inode_lock_map;

semaphore_t file_lock;
semaphore_t open_dir_lock;
semaphore_t inode_lock_lock;
semaphore_t free_data_lock;
semaphore_t free_inode_lock;

superblock_t disk_superblock;

char delim[1] = "/";
char null_term[1] = "\0";
char parent[3] = "..";
char self[2] = ".";

/* -----------------------Inode Lock Logic----------------------------------- */

int lock_block(int blocknum) {
    inode_lock_t l;

    semaphore_P(inode_lock_lock);
    HASH_FIND_INT( inode_lock_map, &blocknum, l );

    if (!l) {
        l = (inode_lock_t) malloc (sizeof(struct inode_lock));
        if (!l) {
            semaphore_V(inode_lock_lock);
            return -1;
        }

        l->mutex = counter_new();
        if (!l->mutex) {
            free(l);
            semaphore_V(inode_lock_lock);
            return -1;
        }
        counter_initialize(l->mutex, 1);
        HASH_ADD_INT( inode_lock_map, blocknum, l );
    }
    semaphore_V(inode_lock_lock);

    counter_P(l->mutex, 1);
    return 0;
}

int unlock_block(int blocknum) {
    inode_lock_t l;

    semaphore_P(inode_lock_lock);
    HASH_FIND_INT( inode_lock_map, &blocknum, l );
    if (!l) {
        semaphore_V(inode_lock_lock);
        return -1;
    }

    if (counter_get_count(l->mutex) <= 0) {
        HASH_DEL( inode_lock_map, l );
        counter_destroy(l->mutex);
        free(l);
    } else { // Someone else waiting
        counter_V(l->mutex, 1);
    }

    semaphore_V(inode_lock_lock);
    return 0;
}

/* -----------------------Current Directory Logic---------------------------- */

void invalidate_dir(int blocknum) {
    dir_list_t d;
    void *f;
    thread_files_t files;

    semaphore_P(open_dir_lock);
    HASH_FIND_INT( open_dir_map, &blocknum, d );
    if (!d) {
        semaphore_V(open_dir_lock);
        return;
    }

    HASH_DEL( open_dir_map, d );

    while (queue_dequeue(d->thread_queue, &f) == 0) {
        files = (thread_files_t) f;
        files->valid = 0;
    }

    queue_free(d->thread_queue);
    free(d);

    semaphore_V(open_dir_lock);
}

void move_dir(thread_files_t files, int new_blocknum) {
    dir_list_t old_dir;
    dir_list_t new_dir;
    int old_blocknum;

    semaphore_P(open_dir_lock);
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
        new_dir->blocknum = new_blocknum;
        new_dir->thread_queue = queue_new();
        queue_append(new_dir->thread_queue, files);
        HASH_ADD_INT( open_dir_map, blocknum, new_dir );
    }
    semaphore_V(open_dir_lock);
}

/* -------------------------------Disk Logic--------------------------------- */

void destroy_waiting_disk(waiting_request_t req) {
    counter_destroy(req->mutex);
    free(req);
}

/* Handler for disk operations
 * Assumes interrupts are disabled within
 */
void minifile_handle(disk_interrupt_arg_t *arg) {
    int blockid;
    char* buffer;
    waiting_request_t req;

    blockid = arg->request.blocknum;
    buffer = arg->request.buffer;
    HASH_FIND_INT( req_map, &blockid, req );

    *(req->reply) = arg->reply;
    semaphore_V(req->wait_for_reply);

    if (counter_get_count(req->mutex) <= 0) {
        HASH_DEL( req_map, req );
        destroy_waiting_disk(req);
    } else { // Someone else waiting
        counter_V(req->mutex, 0); // unsafe v
    }

    free(arg);
}

waiting_request_t create_waiting_disk(int blockid, disk_reply_t* reply) {
    waiting_request_t req;
    semaphore_t wait;

    wait = semaphore_create();
    if (!wait) return NULL;
    semaphore_initialize(wait, 0);

    HASH_FIND_INT( req_map, &blockid, req );
    if (!req) {
        req = (waiting_request_t) malloc (sizeof (struct waiting_request));
        if (!req) {
            free(wait);
            return NULL;
        }

        req->mutex = counter_new();
        if (!req->mutex) {
            free(wait);
            destroy_waiting_disk(req);
            return NULL;
        }
        counter_initialize(req->mutex, 1);

        req->blocknum = blockid;
        HASH_ADD_INT( req_map, blocknum, req );
    }
    counter_P(req->mutex, 1);
    req->reply = reply;
    req->wait_for_reply = wait;

    return req;
}

semaphore_t read_block(int blockid, disk_reply_t* reply, char* buffer) {
    waiting_request_t req = create_waiting_disk(blockid, reply);
    if (!req) return NULL;
    disk_read_block(disk, blockid, buffer);
    return req->wait_for_reply;
}

semaphore_t write_block(int blockid, disk_reply_t* reply, char* buffer) {
    waiting_request_t req = create_waiting_disk(blockid, reply);
    if (!req) return NULL;
    disk_write_block(disk, blockid, buffer);
    return req->wait_for_reply;
}

char* get_block_blocking(int blocknum) {
    semaphore_t wait;
    disk_reply_t reply;
    char* block;

    block = (char *) malloc (DISK_BLOCK_SIZE);

    wait = read_block(blocknum, &reply, block);
    semaphore_P(wait);
    if (reply != DISK_REPLY_OK) {
        free(wait);
        free(block);
        return NULL;
    }

    free(wait);
    return block;
}

int write_block_blocking(int blocknum, char* block) {
    semaphore_t wait;
    disk_reply_t reply;

    wait = write_block(blocknum, &reply, block);
    semaphore_P(wait);
    if (reply != DISK_REPLY_OK) {
        free(wait);
        return -1;
    }

    free(wait);
    return 0;
}

/* ----------------------------Free Blocks----------------------------------- */

char* get_free_inode_block(int *blocknum) {
    free_block_t freeblock;
    int nextblock;

    semaphore_P(free_inode_lock);
    *blocknum = unpack_unsigned_int(disk_superblock->data.first_free_inode);

    if (*blocknum == 0) {
        return NULL;
    } else {
        freeblock = (free_block_t) get_block_blocking(*blocknum);
        nextblock = unpack_unsigned_int(freeblock->next_free_block);
        pack_unsigned_int(disk_superblock->data.first_free_inode, nextblock);
        write_block_blocking(0, (char *) disk_superblock);
    }
    semaphore_V(free_inode_lock);
    return (char *) freeblock;
}

char* get_free_data_block(int *blocknum) {
    free_block_t freeblock;
    int nextblock;

    semaphore_P(free_data_lock);
    *blocknum = unpack_unsigned_int(disk_superblock->data.first_free_data_block);

    if (*blocknum == 0) {
        return NULL;
    } else {
        freeblock = (free_block_t) get_block_blocking(*blocknum);
        nextblock = unpack_unsigned_int(freeblock->next_free_block);
        pack_unsigned_int(disk_superblock->data.first_free_data_block, nextblock);
        write_block_blocking(0, (char *) disk_superblock);
    }
    semaphore_V(free_data_lock);
    return (char *) freeblock;
}

void set_free_inode_block(int blocknum, char *block) {
    free_block_t freeblock;
    int first_free;

    if (blocknum == 0 || block == NULL) {
        return;
    }
    freeblock = (free_block_t) block;
    first_free = unpack_unsigned_int(disk_superblock->data.first_free_inode);
    pack_unsigned_int(freeblock->next_free_block, first_free);
    pack_unsigned_int(disk_superblock->data.first_free_inode, blocknum);

    write_block_blocking(blocknum, block);
    write_block_blocking(0, (char *) disk_superblock);
}

void set_free_data_block(int blocknum, char *block) {
    free_block_t freeblock;
    int first_free;

    if (blocknum == 0 || block == NULL) {
        return;
    }
    freeblock = (free_block_t) block;
    first_free = unpack_unsigned_int(disk_superblock->data.first_free_data_block);
    pack_unsigned_int(freeblock->next_free_block, first_free);
    pack_unsigned_int(disk_superblock->data.first_free_data_block, blocknum);

    write_block_blocking(blocknum, block);
    write_block_blocking(0, (char *) disk_superblock);
}

/* -------------------------------File Logic--------------------------------- */

void truncate_file(inode_t file, int blocknum);

void relink_file(int blocknum) {
    inode_t file_inode;

    file_inode = (inode_t) get_block_blocking(blocknum);
    if (!file_inode) return;

    truncate_file(file_inode, blocknum);
    set_free_inode_block(blocknum, (char *) file_inode);
    free(file_inode);
}

void start_access_file(int blocknum) {
    file_access_t file;

    semaphore_P(file_lock);

    HASH_FIND_INT( file_map, &blocknum, file );
    if (!file) {
        file = (file_access_t) malloc (sizeof (struct file_access));
        file->blocknum = blocknum;
        file->unlinked = 0;
        file->num = 1;
        HASH_ADD_INT( file_map, blocknum, file );
    } else {
        file->num++;
    }

    semaphore_V(file_lock);
}

void unlink_access_file(int blocknum) {
    file_access_t file;

    semaphore_P(file_lock);
    HASH_FIND_INT( file_map, &blocknum, file );
    if (!file) {
        semaphore_V(file_lock);
        return;
    }
    file->unlinked = 1;

    semaphore_V(file_lock);
}

void end_access_file(int blocknum) {
    file_access_t file;
    int unlinked;

    semaphore_P(file_lock);
    HASH_FIND_INT( file_map, &blocknum, file );
    if (!file) {
        semaphore_V(file_lock);
        return;
    }
    file->num--;
    if (file->num == 0) {
        unlinked = file->unlinked;
        HASH_DEL( file_map, file );
        semaphore_V(file_lock);
        free(file);
        if (unlinked) relink_file(blocknum);
    }

    semaphore_V(file_lock);
}

/* -------------------------------------------------------------------------- */

int dir_iterate_indir(indirect_block_t indir, dir_func_t f, void* arg, void* result, int cur_size) {
    unsigned int blockid;
    dir_data_block_t cur_block;
    int acc1;
    int acc2;
    int size;
    //int inode_num;
    int num_to_check;
    indirect_block_t indirect;

    if (cur_size == 0) {
        free(indir);
        return -1;
    }

    size = cur_size;
    for (acc1 = 0; acc1 < DIRECT_PER_TABLE; acc1++) {
        if (size == 0) {
            free(cur_block);
            free(indir);
            return -1;
        }
        blockid = unpack_unsigned_int(indir->data.direct_ptrs[acc1]);
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
        for (acc2 = 0; acc2 < num_to_check; acc2++) {
            //inode_num = unpack_unsigned_int(cur_block->data.inode_ptrs[acc2]);
            if (f(cur_block->data.dir_entries[acc2], cur_block->data.inode_ptrs[acc2], arg, result, blockid, (char *)cur_block) == 0) {
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
    int acc1;
    int acc2;
    int size;
    //int inode_num;
    int num_to_check;
    indirect_block_t indir;

    size = unpack_unsigned_int(dir->data.size);

    for (acc1 = 0; acc1 < DIRECT_BLOCKS; acc1++) {
        if (size == 0) {
            free(cur_block);
            return -1;
        }
        blockid = unpack_unsigned_int(dir->data.direct_ptrs[acc1]);
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
        for (acc2 = 0; acc2 < num_to_check; acc2++) {
            //inode_num = unpack_unsigned_int(cur_block->data.inode_ptrs[acc]);
            if (f(cur_block->data.dir_entries[acc2], cur_block->data.inode_ptrs[acc2], arg, result, blockid, (char *)cur_block) == 0) {
                free(cur_block);
                return 0;
            }
        }
    }
    free(cur_block);
    indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(dir->data.indirect_ptr));

    return dir_iterate_indir(indir, f, arg, result, size);
}

int truncate_iterate_indir(indirect_block_t file, int indir_num, file_func_t f, void* arg, void* result, int cur_size) {
    int acc;
    int size;
    indirect_block_t indirect;
    int temp;

    if (cur_size == 0) {
        free(file);
        return -1;
    }


    size = cur_size;
    for (acc = 0; acc < DIRECT_PER_TABLE; acc++) {
        if (size == 0) {
            return -1;
        }
        if (f((file->data.direct_ptrs[acc]), arg, result) == 0) {
            return 0;
        }
        size = size - 1;
    }

    indirect = (indirect_block_t) get_block_blocking(unpack_unsigned_int(file->data.indirect_ptr));
    temp = unpack_unsigned_int(file->data.indirect_ptr);
    set_free_data_block(indir_num, (char *)file);
    free(file);

    return truncate_iterate_indir(indirect, temp, f, arg, result, size);
}

int truncate_iterate(inode_t file, file_func_t f, void* arg, void* result) {
    int acc;
    int size;
    indirect_block_t indir;

    size = unpack_unsigned_int(file->data.size) / DISK_BLOCK_SIZE;

    if (unpack_unsigned_int(file->data.size) % 4096 != 0) {
        size = size + 1;
    }

    for (acc = 0; acc < DIRECT_BLOCKS; acc++) {
        if (size == 0) {
            return -1;
        }
        if (f(file->data.direct_ptrs[acc], arg, result) == 0) {
            return 0;
        }
        size = size - 1;
    }
    indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(file->data.indirect_ptr));

    return truncate_iterate_indir(indir, unpack_unsigned_int(file->data.indirect_ptr), f, arg, result, size);
}

int get_inode_helper(char *item, char *inode_num, void *arg, void *result, int dummy, char *dummy2) {
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

int make_inode_helper(inode_t dir, int inode_num, char *name, write_func_t f) {
    int direct_ind;
    dir_data_block_t direct_block;
    int direct_blocknum;
    int entry_num;
    int size;
    indirect_block_t indir_block;
    int indir_blocknum;
    indirect_block_t next_indir_block;
    int next_indir_blocknum;
    int new_blocknum;
    inode_t new_block;

    size = unpack_unsigned_int(dir->data.size);
    pack_unsigned_int(dir->data.size, size + 1);
    if (size < DIRECT_BLOCKS * ENTRIES_PER_TABLE) { // found in a direct block
        direct_ind = size / ENTRIES_PER_TABLE;
        entry_num = size % ENTRIES_PER_TABLE;
        direct_blocknum = unpack_unsigned_int(dir->data.direct_ptrs[direct_ind]);
        if (entry_num == 0) { // New direct block
            direct_block = (dir_data_block_t) get_free_data_block(&direct_blocknum);
            if (!direct_block) {
                unlock_block(inode_num);
                free(dir);
                return -1;
            }
            pack_unsigned_int(dir->data.direct_ptrs[direct_ind], direct_blocknum);
        } else if (direct_blocknum == 0) {
            unlock_block(inode_num);
            free(dir);
            return -1;
        } else {
            direct_block = (dir_data_block_t) get_block_blocking(direct_blocknum);
        }

        new_block = (inode_t) get_free_data_block(&new_blocknum);
        if (new_blocknum == -1) {
            if (entry_num == 0) {
                set_free_data_block(direct_blocknum, (char *) direct_block);
            }
            unlock_block(inode_num);
            free(direct_block);
            free(dir);
            return -1;
        }

        strcpy(direct_block->data.dir_entries[entry_num], name);
        pack_unsigned_int(direct_block->data.inode_ptrs[entry_num], new_blocknum);
        write_block_blocking(direct_blocknum, (char *) direct_block);
        write_block_blocking(inode_num, (char *) dir);
        unlock_block(inode_num);
        free(direct_block);
        free(dir);

        return f(new_block, new_blocknum, inode_num);
    }

    size -= DIRECT_BLOCKS * ENTRIES_PER_TABLE; // Remaining size
    if (size == 0) { // first entry in indirect
        indir_block = (indirect_block_t) get_free_data_block(&indir_blocknum);
        if (!indir_block) {
            unlock_block(inode_num);
            free(dir);
            return -1;
        }
        pack_unsigned_int(dir->data.indirect_ptr, indir_blocknum);
    } else {
        indir_blocknum = unpack_unsigned_int(dir->data.indirect_ptr);
        indir_block = (indirect_block_t) get_block_blocking(indir_blocknum);
    }

    while (size >= DIRECT_PER_TABLE * ENTRIES_PER_TABLE) {
        size -= DIRECT_PER_TABLE * ENTRIES_PER_TABLE;
        if (size == 0) { // first entry in indirect
            next_indir_block = (indirect_block_t) get_free_data_block(&next_indir_blocknum);
            if (!next_indir_block) {
                unlock_block(inode_num);
                free(dir);
                return -1;
            }
            pack_unsigned_int(indir_block->data.indirect_ptr, next_indir_blocknum);
            write_block_blocking(indir_blocknum, (char *) indir_block);
        } else {
            next_indir_blocknum = unpack_unsigned_int(indir_block->data.indirect_ptr);
            next_indir_block = (indirect_block_t) get_block_blocking(next_indir_blocknum);
        }

        free(indir_block);
        indir_block = next_indir_block;
        indir_blocknum = next_indir_blocknum;
    }

    direct_ind = size / ENTRIES_PER_TABLE;
    entry_num = size % ENTRIES_PER_TABLE;
    direct_blocknum = unpack_unsigned_int(indir_block->data.direct_ptrs[direct_ind]);

    if (entry_num == 0) { // New direct block
        direct_block = (dir_data_block_t) get_free_data_block(&direct_blocknum);
        if (!direct_block) {
            if (size == 0) {
                set_free_data_block(indir_blocknum, (char *) indir_block);
            }
            unlock_block(inode_num);
            free(indir_block);
            free(dir);
            return -1;
        }
        pack_unsigned_int(indir_block->data.direct_ptrs[direct_ind], direct_blocknum);
    } else if (direct_blocknum == 0) {
        if (size == 0) {
            set_free_data_block(indir_blocknum, (char *) indir_block);
        }
        unlock_block(inode_num);
        free(indir_block);
        free(dir);
        return -1;
    } else {
        direct_block = (dir_data_block_t) get_block_blocking(direct_blocknum);
    }

    new_block = (inode_t) get_free_data_block(&new_blocknum);
    if (new_blocknum == -1) {
        if (entry_num == 0) {
            set_free_data_block(direct_blocknum, (char *) direct_block);
        }
        if (size == 0) {
            set_free_data_block(indir_blocknum, (char *) indir_block);
        }
        unlock_block(inode_num);
        free(direct_block);
        free(dir);
        return -1;
    }

    strcpy(direct_block->data.dir_entries[entry_num], name);
    pack_unsigned_int(direct_block->data.inode_ptrs[entry_num], new_blocknum);
    write_block_blocking(indir_blocknum, (char *) indir_block);
    write_block_blocking(direct_blocknum, (char *) direct_block);
    write_block_blocking(inode_num, (char *) dir);
    unlock_block(inode_num);
    free(indir_block);
    free(direct_block);
    free(dir);

    return f(new_block, new_blocknum, inode_num);
}

int make_inode(char *dirname, write_func_t f) {
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
        if (strlen(name) == 0 || strlen(name) > 256) return -1;
        files = minithread_directory();
        if (!files) return -1;
        prevdir_num = files->inode_num;
    } else {
        name++;
        if (strlen(name) == 0 || strlen(name) > 256) return -1;
        dir = (char *) malloc (name - dirname + 1);
        memcpy(dir, dirname, name - dirname);
        memcpy(dir + (name - dirname), null_term, 1);
        prevdir = get_inode(dir, &prevdir_num);
        free(prevdir);
        free(dir);
    }

    lock_block(prevdir_num);
    prevdir = (inode_t) get_block_blocking(prevdir_num);
    if (!prevdir || prevdir->data.inode_type == FILE_INODE) {
        unlock_block(prevdir_num);
        free(prevdir);
        return -1;
    }

    return make_inode_helper(prevdir, prevdir_num, name, f);
}

int remove_inode(int dir_num, int item);

int truncate_helper(char *item, void *arg, void *result) {
    char *data;
    int num;

    num = unpack_unsigned_int(item);
    data = get_block_blocking(num);
    set_free_data_block(num, data);
    pack_unsigned_int(item, 0);
    return -1;
}

void truncate_file(inode_t file, int blocknum) {
    truncate_iterate(file, truncate_helper, NULL, NULL);
    pack_unsigned_int(file->data.size, 0);
    write_block_blocking(blocknum, (char *) file);
}

int file_read_indir(indirect_block_t file, int start, int cur_size, int maxlen, char *data, int len) {
    int acc;
    int size;
    indirect_block_t indir;
    char *data_block;
    int current_block;
    int req_left;
    int amt;
    int block_start;
    int block_size;
    int total_copied;

    total_copied = len;
    current_block = start / DISK_BLOCK_SIZE;
    req_left = maxlen;
    size = cur_size;
    block_start = start % DISK_BLOCK_SIZE;
    if (size == 0 || req_left == 0) return 0;
    if (current_block < DIRECT_PER_TABLE) {
        for (acc = current_block; acc < DIRECT_PER_TABLE; acc++) {
            if (size > DISK_BLOCK_SIZE) {
                block_size = DISK_BLOCK_SIZE - block_start;
            } else {
                block_size = size;
            }
            if (req_left < block_size) {
                amt = req_left;
                req_left = 0;
                size -= amt;
            } else {
                amt = block_size;
                req_left -= (block_size);
                size -= amt;
            }
            data_block = get_block_blocking(unpack_unsigned_int(file->data.direct_ptrs[acc]));
            if (!data_block) return -1;
            memcpy(data+total_copied, data_block+block_start, amt);
            total_copied += amt;
            block_start = 0;

            free(data_block);
            if (size == 0 || req_left == 0) {
                return total_copied;
            }

        }

    } else {
        block_start = start - (DISK_BLOCK_SIZE * DIRECT_PER_TABLE);
    }

    indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(file->data.indirect_ptr));

    return file_read_indir(indir, block_start, size, req_left, data, total_copied);

}

int file_read(inode_t file, int start, int maxlen, char *data) {
    int acc;
    int size;
    // int size_block;
    indirect_block_t indir;
    char *data_block;
    int current_block;
    int req_left;
    int amt;
    int block_start;
    int block_size;
    int total_copied;

    total_copied = 0;
    current_block = start / DISK_BLOCK_SIZE;
    req_left = maxlen;
    size = unpack_unsigned_int(file->data.size);
    // size_block = unpack_unsigned_int(file->data.size) / DISK_BLOCK_SIZE;

    // if (unpack_unsigned_int(file->data.size) % DISK_BLOCK_SIZE != 0) {
    //     size_block = size_block + 1;
    // }

    size = size - start;
    // size_block = size_block - current_block;
    block_start = start % DISK_BLOCK_SIZE;
    if (size == 0 || req_left == 0) return 0;
    if (current_block < DIRECT_BLOCKS) {
        for (acc = current_block; acc < DIRECT_BLOCKS; acc++) {
            if (size > DISK_BLOCK_SIZE) {
                block_size = DISK_BLOCK_SIZE - block_start;
            } else {
                block_size = size;
            }
            if (req_left < block_size) {
                amt = req_left;
                req_left = 0;
                size -= amt;
            } else {
                amt = block_size;
                req_left -= (block_size);
                size -= amt;
            }
            data_block = get_block_blocking(unpack_unsigned_int(file->data.direct_ptrs[acc]));
            if (!data_block) return -1;
            memcpy(data+total_copied, data_block+block_start, amt);
            total_copied += amt;
            block_start = 0;

            free(data_block);
            if (size == 0 || req_left == 0) {
                return total_copied;
            }

        }

    } else {
        block_start = start - (DISK_BLOCK_SIZE * DIRECT_BLOCKS);
    }

    indir = (indirect_block_t) get_block_blocking(unpack_unsigned_int(file->data.indirect_ptr));

    return file_read_indir(indir, block_start, size, req_left, data, total_copied);
}

/* Initialize minifile globals */
void minifile_initialize() {
    file_map = NULL;
    req_map = NULL;
    open_dir_map = NULL;
    inode_lock_map = NULL;

    file_lock = semaphore_create();
    open_dir_lock = semaphore_create();
    inode_lock_lock = semaphore_create();
    free_data_lock = semaphore_create();
    free_inode_lock = semaphore_create();

    semaphore_initialize(file_lock, 1);
    semaphore_initialize(open_dir_lock, 1);
    semaphore_initialize(inode_lock_lock, 1);
    semaphore_initialize(free_data_lock, 1);
    semaphore_initialize(free_inode_lock, 1);

    disk_superblock = (superblock_t) malloc (sizeof(struct superblock));
}

/* Initialize minifile global blocks */
void minifile_initialize_blocks() {
    int magic_num;

    if (!use_existing_disk) return;
    disk_superblock = (superblock_t) get_block_blocking(0);

    magic_num = unpack_unsigned_int(disk_superblock->data.magic_number);
    if (magic_num != MAGIC) {
        printf("Invalid magic number\n");
        exit(0);
        return;
    }
}

int minifile_get_root_num() {
    return unpack_unsigned_int(disk_superblock->data.root_inode);
}

int write_new_file(inode_t newfile, int newfile_num, int prevdir) {
    int i;

    newfile->data.inode_type = FILE_INODE;
    pack_unsigned_int(newfile->data.size, 0);
    for (i = 0; i < DIRECT_BLOCKS; i++) {
        pack_unsigned_int(newfile->data.direct_ptrs[i], 0);
    }
    pack_unsigned_int(newfile->data.indirect_ptr, 0);

    write_block_blocking(newfile_num, (char *) newfile);

    free(newfile);
    return newfile_num;
}

minifile_t minifile_creat(char *filename) {
    char mode[2] = "w";
    return minifile_open(filename, mode);
}

minifile_t minifile_open(char *filename, char *mode) {
    int inode_num;
    inode_t inode;
    minifile_t file;
    thread_files_t files;
    char *filename_copy;

    if (!mode) return NULL;

    filename_copy = (char *) malloc (strlen(filename) + 1);
    if (!filename_copy) return NULL;
    memcpy(filename_copy, filename, strlen(filename) + 1);

    inode = get_inode(filename_copy, &inode_num);
    free(filename_copy);
    if (inode && inode->data.inode_type == DIR_INODE) return NULL;

    if ((strcmp(mode, "r") == 0) || (strcmp(mode, "r+") == 0)) {
        if (!inode) return NULL;
    } else if ((strcmp(mode, "w") == 0) || (strcmp(mode, "w+") == 0)) {
        if (!inode) {
            inode_num = make_inode(filename, write_new_file);
            if (inode_num == -1) return NULL;
        } else {
            truncate_file(inode, inode_num);
        }
    } else if ((strcmp(mode, "a") == 0) || (strcmp(mode, "a+") == 0)) {
        if (!inode) {
            inode_num = make_inode(filename, write_new_file);
            if (inode_num == -1) return NULL;
        }
    }

    file = (minifile_t) malloc (sizeof(struct minifile));
    file->inode_num = inode_num;
    if ((strcmp(mode, "w") == 0) || (strcmp(mode, "a") == 0)) {
        file->readable = 0;
    } else {
        file->readable = 1;
    }

    if (strcmp(mode, "r") == 0) {
        file->writeable = 0;
    } else {
        file->writeable = 1;
    }

    if ((strcmp(mode, "a") == 0) || (strcmp(mode, "a+") == 0)) {
        file->append = 1;
    } else {
        file->append = 0;
    }

    file->cursor = 0;

    files = minithread_directory();
    queue_append(files->open_files, file);
    start_access_file(inode_num);

    return file;
}

int minifile_read(minifile_t file, char *data, int maxlen) {
    int read;
    inode_t block;
    if (!file->readable) return -1;
    block = (inode_t) get_block_blocking(file->inode_num);

    read = file_read(block, file->cursor, maxlen, data);
    if (read == -1) {
        free(block);
        return -1;
    }
    file->cursor += read;
    free(block);
    return read;
}

int file_write_indir(indirect_block_t file, int file_num, int start, int cur_size, char *data, int len, int written) {
    int block_start;
    int req_left;
    int total_written;
    int acc;
    int amt;
    int byte_start;
    int next_start;
    int blockid;
    int block_size;
    char *block;
    indirect_block_t indir;
    int indir_num;

    total_written = written;
    req_left = len;

    block_start = start / DISK_BLOCK_SIZE;
    byte_start = start % DISK_BLOCK_SIZE;
    block_size = cur_size;

    if (block_start < DIRECT_PER_TABLE) {
        for (acc = block_start; acc < DIRECT_PER_TABLE; acc++) {
            if (req_left > (DISK_BLOCK_SIZE - byte_start)) {
                amt = (DISK_BLOCK_SIZE - byte_start);
                byte_start = 0;
                req_left -= amt;
            } else {
                amt = req_left;
                req_left = 0;
            }

            if (block_size == 0) {
                block = get_free_data_block(&blockid);
                if (!block) return -1;
                pack_unsigned_int(file->data.direct_ptrs[acc], blockid);
                write_block_blocking(file_num, (char *)file);
            } else {
                blockid = unpack_unsigned_int(file->data.direct_ptrs[acc]);
                block = get_block_blocking(blockid);
                if (!block) return -1;
                block_size -= 1;
            }

            memcpy(block+byte_start, data+total_written, amt);
            write_block_blocking(blockid, block);
            byte_start = 0;
            total_written += amt;
            free(block);

            if (req_left == 0) {
                return total_written;
            }
        }
        next_start = 0;
    } else {
        next_start = start - (DIRECT_PER_TABLE * DISK_BLOCK_SIZE);
        block_size -= DIRECT_PER_TABLE;
    }

    if (block_size == 0) {
        indir = (indirect_block_t) get_free_data_block(&indir_num);
        if (!indir) return -1;
        pack_unsigned_int(file->data.indirect_ptr, indir_num);
        write_block_blocking(file_num, (char *)file);
    } else {
        indir_num = unpack_unsigned_int(file->data.indirect_ptr);
        indir = (indirect_block_t) get_block_blocking(indir_num);
        if (!indir) return -1;
    }
    free(file);

    return file_write_indir(indir, indir_num, next_start, block_size, data, req_left, total_written);
}

int file_write(inode_t file, int file_num, int start, char *data, int len) {
    int size;
    int block_start;
    int req_left;
    int total_written;
    int acc;
    int amt;
    int byte_start;
    int next_start;
    int blockid;
    int block_size;
    char *block;
    indirect_block_t indir;
    int indir_num;

    total_written = 0;
    req_left = len;

    size = unpack_unsigned_int(file->data.size);
    block_start = start / DISK_BLOCK_SIZE;
    byte_start = start % DISK_BLOCK_SIZE;
    block_size = size / DISK_BLOCK_SIZE;
    if (size % DISK_BLOCK_SIZE != 0) {
        block_size ++;
    }

    if (block_start < DIRECT_BLOCKS) {
        for (acc = block_start; acc < DIRECT_BLOCKS; acc++) {
            if (req_left > (DISK_BLOCK_SIZE - byte_start)) {
                amt = (DISK_BLOCK_SIZE - byte_start);
                byte_start = 0;
                req_left -= amt;
            } else {
                amt = req_left;
                req_left = 0;
            }

            if (block_size == 0) {
                block = get_free_data_block(&blockid);
                if (!block) return -1;
                pack_unsigned_int(file->data.direct_ptrs[acc], blockid);
                write_block_blocking(file_num, (char *)file);
            } else {
                blockid = unpack_unsigned_int(file->data.direct_ptrs[acc]);
                block = get_block_blocking(blockid);
                if (!block) return -1;
                block_size -= 1;
            }
            memcpy(block+byte_start, data+total_written, amt);
            write_block_blocking(blockid, block);
            byte_start = 0;
            total_written += amt;
            free(block);
            if (req_left == 0) {
                return total_written;
            }
        }
        next_start = 0;
    } else {
        next_start = start - (DIRECT_BLOCKS * DISK_BLOCK_SIZE);
        block_size -= DIRECT_BLOCKS;
    }
    if (block_size == 0) {
        indir = (indirect_block_t) get_free_data_block(&indir_num);
        if (!indir) return -1;
        pack_unsigned_int(file->data.indirect_ptr, indir_num);
        write_block_blocking(file_num, (char *)file);
    } else {
        indir_num = unpack_unsigned_int(file->data.indirect_ptr);
        indir = (indirect_block_t) get_block_blocking(indir_num);
        if (!indir) return -1;
    }

    return file_write_indir(indir, indir_num, next_start, block_size, data, req_left, total_written);
}

int file_write_blocks_indir(indirect_block_t file, int file_num, int start, int cur_size, int len, int written) {
    int block_start;
    int req_left;
    int total_written;
    int acc;
    int amt;
    int byte_start;
    int next_start;
    int blockid;
    int block_size;
    char *block;
    indirect_block_t indir;
    int indir_num;

    total_written = written;
    req_left = len;

    block_start = start / DISK_BLOCK_SIZE;
    byte_start = start % DISK_BLOCK_SIZE;
    block_size = cur_size;

    if (block_start < DIRECT_PER_TABLE) {
        for (acc = block_start; acc < DIRECT_PER_TABLE; acc++) {
            if (req_left > (DISK_BLOCK_SIZE - byte_start)) {
                amt = (DISK_BLOCK_SIZE - byte_start);
                byte_start = 0;
                req_left -= amt;
            } else {
                amt = req_left;
                req_left = 0;
            }

            if (block_size == 0) {
                block = get_free_data_block(&blockid);
                if (!block) return -1;
                pack_unsigned_int(file->data.direct_ptrs[acc], blockid);
            } else {
                blockid = unpack_unsigned_int(file->data.direct_ptrs[acc]);
                block = get_block_blocking(blockid);
                if (!block) return -1;
                block_size -= 1;
            }

            byte_start = 0;
            total_written += amt;
            free(block);

            if (req_left == 0) {
                write_block_blocking(file_num, (char *)file);
                return total_written;
            }
        }
        next_start = 0;
    } else {
        next_start = start - (DIRECT_PER_TABLE * DISK_BLOCK_SIZE);
        block_size -= DIRECT_PER_TABLE;
    }

    if (block_size == 0) {
        indir = (indirect_block_t) get_free_data_block(&indir_num);
        if (!indir) return -1;
        pack_unsigned_int(file->data.indirect_ptr, indir_num);
    } else {
        indir_num = unpack_unsigned_int(file->data.indirect_ptr);
        indir = (indirect_block_t) get_block_blocking(indir_num);
        if (!indir) return -1;
    }
    write_block_blocking(file_num, (char *)file);
    free(file);

    return file_write_blocks_indir(indir, indir_num, next_start, block_size, req_left, total_written);
}

int file_write_blocks(inode_t file, int file_num, int start, int len) {
    int size;
    int block_start;
    int req_left;
    int total_written;
    int acc;
    int amt;
    int byte_start;
    int next_start;
    int blockid;
    int block_size;
    char *block;
    indirect_block_t indir;
    int indir_num;

    total_written = 0;
    req_left = len;

    size = unpack_unsigned_int(file->data.size);
    block_start = start / DISK_BLOCK_SIZE;
    byte_start = start % DISK_BLOCK_SIZE;
    block_size = size / DISK_BLOCK_SIZE;
    if (size % DISK_BLOCK_SIZE != 0) {
        block_size ++;
    }

    if (block_start < DIRECT_BLOCKS) {
        for (acc = block_start; acc < DIRECT_BLOCKS; acc++) {
            if (req_left > (DISK_BLOCK_SIZE - byte_start)) {
                amt = (DISK_BLOCK_SIZE - byte_start);
                byte_start = 0;
                req_left -= amt;
            } else {
                amt = req_left;
                req_left = 0;
            }

            if (block_size == 0) {
                block = get_free_data_block(&blockid);
                if (!block) return -1;
                pack_unsigned_int(file->data.direct_ptrs[acc], blockid);
            } else {
                blockid = unpack_unsigned_int(file->data.direct_ptrs[acc]);
                block = get_block_blocking(blockid);
                if (!block) return -1;
                block_size -= 1;
            }

            byte_start = 0;
            total_written += amt;
            free(block);
            if (req_left == 0) {
                write_block_blocking(file_num, (char *)file);
                return total_written;
            }
        }
        next_start = 0;
    } else {
        next_start = start - (DIRECT_BLOCKS * DISK_BLOCK_SIZE);
        block_size -= DIRECT_BLOCKS;
    }
    if (block_size == 0) {
        indir = (indirect_block_t) get_free_data_block(&indir_num);
        if (!indir) return -1;
        pack_unsigned_int(file->data.indirect_ptr, indir_num);
    } else {
        indir_num = unpack_unsigned_int(file->data.indirect_ptr);
        indir = (indirect_block_t) get_block_blocking(indir_num);
        if (!indir) return -1;
    }
    write_block_blocking(file_num, (char *)file);

    return file_write_blocks_indir(indir, indir_num, next_start, block_size, req_left, total_written);
}

int minifile_write(minifile_t file, char *data, int len) {
    int write;
    inode_t block;
    int size;
    int temp;
    if (!file->writeable) return -1;

    block = (inode_t) get_block_blocking(file->inode_num);
    if (!block) return -1;
    if (file->append) {
        file->cursor = unpack_unsigned_int(block->data.size);
    }
    if (lock_block(file->inode_num) == -1) {
        unlock_block(file->inode_num);
        free(block);
        return -1;
    }
    write = file_write_blocks(block, file->inode_num, file->cursor, len);
    if (write == -1) {
        unlock_block(file->inode_num);
        free(block);
        return -1;
    }
    size = unpack_unsigned_int(block->data.size);
    temp = file->cursor;
    file->cursor += write;
    if (file->cursor > size) {
        pack_unsigned_int(block->data.size, file->cursor);
        write_block_blocking(file->inode_num, (char *)block);
    }
    unlock_block(file->inode_num);
    write = file_write(block, file->inode_num, temp, data, len);
    if (write == -1) {
        free(block);
        return -1;
    }
    file->cursor = temp + write;
    free(block);
    return 0;
}

int minifile_close(minifile_t file) {
    end_access_file(file->inode_num);
    free(file);
    return 0;
}

int minifile_unlink(char *filename) {
    inode_t file_inode;
    int blocknum;
    thread_files_t files;
    char *dir;
    char *name;
    inode_t prevdir;
    int prevdir_num;
    file_access_t file;
    char *filename_copy;

    filename_copy = (char *) malloc (strlen(filename) + 1);
    if (!filename_copy) return -1;
    memcpy(filename_copy, filename, strlen(filename) + 1);

    file_inode = get_inode(filename_copy, &blocknum);
    free(filename_copy);
    if (!file_inode || file_inode->data.inode_type == DIR_INODE) return -1;

    name = strrchr(filename, '/');

    if (!name) { // Current directory
        name = filename;
        files = minithread_directory();
        if (!files) {
            free(file_inode);
            return -1;
        }
        prevdir = (inode_t) get_block_blocking(files->inode_num);
        prevdir_num = files->inode_num;
    } else {
        name++;
        dir = (char *) malloc (name - filename + 1);
        memcpy(dir, filename, name - filename);
        memcpy(dir + (name - filename), null_term, 1);
        prevdir = get_inode(dir, &prevdir_num);
        free(dir);
    }
    if (!prevdir || prevdir->data.inode_type == FILE_INODE) {
        free(file_inode);
        free(prevdir);
        return -1;
    }
    lock_block(prevdir_num);
    if (remove_inode(prevdir_num, blocknum) == -1)  {
        unlock_block(prevdir_num);
        return -1;
    }
    unlock_block(prevdir_num);

    //removed from parent
    HASH_FIND_INT( file_map, &blocknum, file );
    if (!file) {
        unlink_access_file(blocknum);
    } else {
        relink_file(blocknum);
    }

    free(file_inode);
    free(prevdir);
    return 0;
}

int write_new_dir(inode_t newdir, int newdir_num, int prevdir) {
    dir_data_block_t newdir_data;
    int newdir_data_num;
    int i;

    newdir_data = (dir_data_block_t) get_free_data_block(&newdir_data_num);

    if (!newdir_data) {
        set_free_data_block(newdir_data_num, (char *) newdir_data);
        free(newdir_data);
        free(newdir);
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

    write_block_blocking(newdir_data_num, (char *) newdir_data);
    write_block_blocking(newdir_num, (char *) newdir);

    free(newdir_data);
    free(newdir);
    return newdir_num;
}

int minifile_mkdir(char *dirname) {
    return make_inode(dirname, write_new_dir);
}


int rm_last(inode_t dir, int dir_num, char **result_num, char **result) {
    unsigned int blockid;
    dir_data_block_t cur_block;
    int size;
    indirect_block_t indir;
    int indir_num;
    indirect_block_t temp;
    int temp_num;
    int transfer;

    size = unpack_unsigned_int(dir->data.size);
    if (((size - 1) / ENTRIES_PER_TABLE) < DIRECT_BLOCKS) {
        blockid = unpack_unsigned_int(dir->data.direct_ptrs[(size -1) / ENTRIES_PER_TABLE]);
        if (blockid == 0) {
            return -1;
        }
        cur_block = (dir_data_block_t) get_block_blocking(blockid);
        *result_num = (char *) malloc (4);
        if (!(*result_num)) {
            free(cur_block);
            return -1;
        }
        transfer = unpack_unsigned_int(cur_block->data.inode_ptrs[(size - 1) % ENTRIES_PER_TABLE]);
        pack_unsigned_int(*result_num, transfer);
        *result = (char *) malloc (257);
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
            *result_num = (char *) malloc (4);
            if (!(*result_num)) {
                free(cur_block);
                free(indir);
                free(temp);
                return -1;
            }
            transfer = unpack_unsigned_int(cur_block->data.inode_ptrs[(size - 1) % ENTRIES_PER_TABLE]);
            pack_unsigned_int(*result_num, transfer);
            *result = (char *) malloc (257);
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


int remove_inode_helper(char *item, char *inode_num, void *arg, void *result, int blockid, char *block) {
    int number;
    int *num;
    char *name;
    int actual_num;
    num_struct_t nstruct;

    nstruct = (num_struct_t) result;

    name = nstruct->structure;
    number = unpack_unsigned_int(nstruct->num);

    actual_num = unpack_unsigned_int(inode_num);
    num = (int *)arg;

    if (*num == actual_num) {
        memcpy(item, name, strlen(name)+1);
        pack_unsigned_int(inode_num, number);
        write_block_blocking(blockid, block);
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
    return 0;
}

int minifile_rmdir(char *dirname) {
    inode_t block;
    int blockid;
    int size;
    int parent_num;
    dir_data_block_t freed_data;
    char *dirname_copy;

    dirname_copy = (char *) malloc (strlen(dirname) + 1);
    if (!dirname_copy) return -1;
    memcpy(dirname_copy, dirname, strlen(dirname) + 1);

    if ((strcmp(dirname_copy, "..") == 0) || (strcmp(dirname_copy, ".") == 0)) {
        free(dirname_copy);
        return -1;
    }
    block = get_inode(dirname_copy, &blockid);
    free(dirname_copy);

    if (!block) return -1;
    size = unpack_unsigned_int(block->data.size);
    if (block->data.inode_type == FILE_INODE || size > 2) {
        free(block);
        return -1;
    }

    freed_data = (dir_data_block_t) get_block_blocking(unpack_unsigned_int(block->data.direct_ptrs[0]));

    parent_num = unpack_unsigned_int(freed_data->data.inode_ptrs[0]);
    lock_block(parent_num);
    if (remove_inode(parent_num, blockid) == -1) {
        unlock_block(parent_num);
        free(block);
        free(freed_data);
        return -1;
    }
    unlock_block(parent_num);

    invalidate_dir(blockid);

    set_free_data_block(unpack_unsigned_int(block->data.direct_ptrs[0]), (char *)freed_data);
    set_free_inode_block(blockid, (char *)block);

    free(block);
    free(freed_data);


    return 0;
}

int minifile_stat(char *path) {
    inode_t block;
    int dummy;
    int size;
    char *path_copy;

    path_copy = (char *) malloc (strlen(path) + 1);
    if (!path_copy) return -1;
    memcpy(path_copy, path, strlen(path) + 1);

    block = get_inode(path_copy, &dummy);
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
    if (!path_copy) return -1;
    memcpy(path_copy, path, path_len+1);

    block = get_inode(path_copy, &inode_num);
    memcpy(path_copy, path, path_len+1);
    if (!block || block->data.inode_type == FILE_INODE) {
        free(block);
        free(path_copy);
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

    move_dir(minithread_directory(), inode_num);
    files->inode_num = inode_num;

    free(path_copy);
    return 0;
}

int ls_helper(char *item, char *inode_num, void *arg, void *result, int dummy, char *dummy2) {
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
    char *path_copy;

    path_copy = (char *) malloc (strlen(path) + 1);
    if (!path_copy) return NULL;
    memcpy(path_copy, path, strlen(path) + 1);

    if (!path || strlen(path) == 0) {
        files = minithread_directory();
        if (!files || files->valid == 0) return NULL;
        block = (inode_t) get_block_blocking(files->inode_num);
    } else {
        block = get_inode(path_copy, &dummy);
    }
    free(path_copy);

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

void minifile_clearpath(thread_files_t files) {
    void *str_ptr;
    str_and_len_t str;
    void *f;
    minifile_t file;
    dir_list_t dir;

    HASH_FIND_INT( open_dir_map, &files->inode_num, dir );
    if (dir) {
        queue_delete(dir->thread_queue, files);
        if (queue_length(dir->thread_queue) == 0) { // Remove if last element
            HASH_DEL( open_dir_map, dir );
            queue_free(dir->thread_queue);
            free(dir);
        }
    }

    while (queue_dequeue(files->path, &str_ptr) == 0) {
        str = (str_and_len_t) str_ptr;
        free(str->data);
        free(str);
    }
    while (queue_dequeue(files->open_files, &f) == 0) {
        file = (minifile_t) f;
        minifile_close(file);
    }

    queue_free(files->open_files);
    queue_free(files->path);
}
