#include "minithread.h"
#include "synch.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disk.h"
#include "minifile.h"
#include "miniheader.h"

#define DISK_SIZE 1000

int initialize_free_blocks(int start, int end) {
    int i;
    int prev_ind;
    free_block_t freeblock;
    waiting_request_t req;

    freeblock = (free_block_t) malloc (sizeof (struct free_block));

    prev_ind = 0;
    for (i = end; i > start - 1; i--) {
        pack_unsigned_int(freeblock->next_free_block, prev_ind);
        prev_ind = i;

        req = write_block(i, (char *) freeblock);
        semaphore_P(req->wait);
        if (req->reply != DISK_REPLY_OK) {
            printf("Failed to write free block: %i\n", i);
            free(freeblock);
            free(req);
            return -1;
        }
        free(req);
    }

    free(freeblock);
    return 0;
}

// Initialize root directory
// Returns -1 on failure
int initialize_root_dir(int max_inode_index) {
    inode_t root_inode;
    dir_data_block_t root_dir;
    char parent[2] = "..";
    char self[1] = ".";
    int i;
    waiting_request_t req;

    root_dir = (dir_data_block_t) malloc (sizeof (struct dir_data_block));

    // Construct directory
    pack_unsigned_int(root_dir->data.inode_ptrs[0], 1);
    memcpy(root_dir->data.dir_entries[0], parent, 3);
    pack_unsigned_int(root_dir->data.inode_ptrs[1], 1);
    memcpy(root_dir->data.dir_entries[1], self, 2);
    for (i = 2; i < ENTRIES_PER_TABLE; i++) {
        pack_unsigned_int(root_dir->data.inode_ptrs[i], 0);
    }

    req = write_block(max_inode_index + 1, (char *) root_dir);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to write root directory\n");
        free(root_dir);
        free(req);
        return -1;
    }
    free(root_dir);
    free(req);
    
    root_inode = (inode_t) malloc (sizeof (struct inode));

    // Construct new inode
    root_inode->data.inode_type = DIR_INODE;
    pack_unsigned_int(root_inode->data.size, 2);
    pack_unsigned_int(root_inode->data.direct_ptrs[0], max_inode_index + 1);
    for (i = 1; i < DIRECT_BLOCKS; i++) {
        pack_unsigned_int(root_inode->data.direct_ptrs[i], 0);
    }
    pack_unsigned_int(root_inode->data.indirect_ptr, 0);

    req = write_block(1, (char *) root_inode);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to write root directory\n");
        free(root_inode);
        free(req);
        return -1;
    }
    free(root_inode);
    free(req);

    return 0;
}

// Constructs a file system
int mkfs(int *arg) {
    int size;
    int max_inode_index;
    superblock_t sprblk;
    waiting_request_t req;

    size = disk->layout.size;
    max_inode_index = RATIO_INODE * size;

    // Initialize superblock
    sprblk = (superblock_t) malloc (sizeof (struct superblock));
    pack_unsigned_int(sprblk->data.magic_number, MAGIC);
    pack_unsigned_int(sprblk->data.disk_size, size);
    pack_unsigned_int(sprblk->data.root_inode, 0);
    pack_unsigned_int(sprblk->data.first_free_inode, 0);
    pack_unsigned_int(sprblk->data.first_free_data_block, 0);

    printf("Writing free inodes\n");
    if (initialize_free_blocks(2, max_inode_index) == -1) {
        return -1;
    }
    pack_unsigned_int(sprblk->data.first_free_inode, 2);
    printf("Successfully written free inodes\n");

    printf("Writing free data blocks\n");
    if (initialize_free_blocks(max_inode_index + 2, size - 1) == -1) {
        return -1;
    }
    pack_unsigned_int(sprblk->data.first_free_data_block, max_inode_index + 2);
    printf("Successfully written free data blocks\n");

    printf("Writing root directory\n");
    if (initialize_root_dir(max_inode_index) == -1) {
        return -1;
    }
    pack_unsigned_int(sprblk->data.root_inode, 1);
    printf("Successfully written root directory\n");

    printf("Writing superblock\n");
    req = write_block(0, (char *) sprblk);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to write superblock\n");
        free(sprblk);
        free(req);
        return -1;
    }
    free(sprblk);
    free(req);
    printf("Successfully written superblock\n");

    printf("File system created\n");
    return 0;
}

int main(int argc, char** argv) {
    use_existing_disk = 0;
    disk_name = "MINIFILESYSTEM";
    disk_flags = DISK_READWRITE;
    disk_size = DISK_SIZE;

    minithread_system_initialize(mkfs, NULL);
    return -1;
}
