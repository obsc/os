#include "minithread.h"
#include "synch.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disk.h"
#include "minifile.h"
#include "miniheader.h"

int size;
int max_inode_index;
superblock_t sprblk;

int root_inode_num;
int free_inode;
int free_data_block;

// Validates the superblock
// Returns -1 on failure
int validate_superblock() {
    waiting_request_t req;

    req = read_block(0, (char *) sprblk);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to read superblock\n");
        free(req);
        return -1;
    }
    free(req);

    if (unpack_unsigned_int(sprblk->data.magic_number) != MAGIC) {
        printf("Invalid magic number in superblock\n");
        return -1;
    }
    if (unpack_unsigned_int(sprblk->data.disk_size) != size) {
        printf("Invalid size in superblock\n");
        return -1;
    }

    root_inode_num = unpack_unsigned_int(sprblk->data.root_inode);
    free_inode = unpack_unsigned_int(sprblk->data.first_free_inode);
    free_data_block = unpack_unsigned_int(sprblk->data.first_free_data_block);

    return 0;
}

int fsck(int *arg) {
    size = disk->layout.size;
    max_inode_index = RATIO_INODE * size;

    sprblk = (superblock_t) malloc (sizeof (struct superblock));

    printf("Validating superblock\n");
    if (validate_superblock() == -1) return -1;


    printf("File system validated\n");
    return 0;
}

int main(int argc, char** argv) {
    use_existing_disk = 1;
    disk_name = "MINIFILESYSTEM";

    minithread_system_initialize(fsck, NULL);
    return -1;
}
