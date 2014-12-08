#include "minithread.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disk.h"
#include "minifile.h"
#include "miniheader.h"

#define DISK_SIZE 1000

// Initialize all the free inodes
// Returns -1 on failure
int initialize_inodes(int max_inode_index) {
    int i;
    int prev_ind;
    free_block_t freeblock;
    waiting_request_t req;

    prev_ind = 0;
    for (i = max_inode_index; i > 1; i--) {
        freeblock = (free_block_t) malloc (sizeof (struct free_block));
        pack_unsigned_int(freeblock->next_free_block, prev_ind);
        prev_ind = i;

        req = write_block(i, (char *) freeblock);
        semaphore_P(req->wait);
        if (req->reply == DISK_REPLY_OK) {

        } else {
            printf("Failed to write free inode block: %i\n", i);
            return -1;
        }
    }

    return 0;
}

int mkfs(int *arg) {
    int size;
    int max_inode_index;
    superblock_t sprblk;

    size = disk->layout.size;
    max_inode_index = RATIO_INODE * size;

    // Initialize superblock
    sprblk = (superblock_t) malloc (sizeof (struct superblock));
    pack_unsigned_int(sprblk->data.magic_number, MAGIC);
    pack_unsigned_int(sprblk->data.disk_size, size);

    if (initialize_inodes(max_inode_index) == -1) {
        return -1;
    }
    pack_unsigned_int(sprblk->data.first_free_inode, 2);

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
