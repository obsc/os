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

char* inode_flags;

// Validates the superblock
int validate_superblock() {
    waiting_request_t req;
    int dirty;

    dirty = 0;

    req = read_block(0, (char *) sprblk);
    semaphore_P(req->wait);
    if (req->reply != DISK_REPLY_OK) {
        printf("Failed to read superblock\n");
        free(req);
        return -1;
    }
    free(req);

    // Check magic number
    if (unpack_unsigned_int(sprblk->data.magic_number) != MAGIC) {
        printf("Invalid magic number in superblock, fixing error\n");
        pack_unsigned_int(sprblk->data.magic_number, MAGIC);
        dirty = 1;
    }
    // Check disk size
    if (unpack_unsigned_int(sprblk->data.disk_size) != size) {
        printf("Invalid size in superblock, fixing error\n");
        pack_unsigned_int(sprblk->data.disk_size, size);
        dirty = 1;
    }

    // Get inode pointers
    root_inode_num = unpack_unsigned_int(sprblk->data.root_inode);
    free_inode = unpack_unsigned_int(sprblk->data.first_free_inode);
    free_data_block = unpack_unsigned_int(sprblk->data.first_free_data_block);

    if (dirty) {
        req = write_block(0, (char *) sprblk);
        semaphore_P(req->wait);
        if (req->reply != DISK_REPLY_OK) {
            printf("Failed to write to superblock\n");
            free(req);
            return -1;
        }
        free(req);
    }

    return 0;
}

// Validates the free inodes linked list
int validate_free_inodes() {
    free_block_t prevblock;
    free_block_t nextblock;
    int previd;
    int nextid;
    waiting_request_t req;
    int dirty;

    dirty = 0;
    nextid = free_inode;
    prevblock = NULL;
    nextblock = NULL;

    while (nextid != 0) {
        if (nextid > max_inode_index) { // inode pointing to data
            printf("Free inode pointing to data block, truncating list\n");
            dirty = 1;
        } else if (inode_flags[nextid] == 1) { // cycle
            printf("Free inode list has a cyle, truncating list\n");
            dirty = 1;
        }

        if (dirty) {
            if (prevblock == NULL) { // First block after superblock
                pack_unsigned_int(sprblk->data.first_free_inode, 0);
                req = write_block(0, (char *) sprblk);
                semaphore_P(req->wait);
                if (req->reply != DISK_REPLY_OK) {
                    printf("Failed to write to superblock\n");
                    free(req);
                    return -1;
                }
                free(req);
                return 0;
            } else { // Some block in the list
                pack_unsigned_int(prevblock->next_free_block, 0);
                req = write_block(previd, (char *) prevblock);
                if (req->reply != DISK_REPLY_OK) {
                    printf("Failed to write to free inode block: %i\n", previd);
                    free(req);
                    return -1;
                }
                free(req);
                return 0;
            }
        }

        nextblock = (free_block_t) malloc (sizeof (struct free_block));

        req = read_block(nextid, (char *) nextblock);
        semaphore_P(req->wait);
        if (req->reply != DISK_REPLY_OK) {
            printf("Failed to read free inode block: %i\n", nextid);
            free(req);
            return -1;
        }
        free(req);

        inode_flags[nextid] = 1;

        free(prevblock);
        prevblock = nextblock;
        previd = nextid;
        nextid = unpack_unsigned_int(nextblock->next_free_block);
    }

    free(nextblock);
    return 0;
}

int fsck(int *arg) {
    size = disk->layout.size;
    max_inode_index = RATIO_INODE * size;

    sprblk = (superblock_t) malloc (sizeof (struct superblock));
    inode_flags = (char *) calloc (size, 1);

    printf("Validating superblock\n");
    if (validate_superblock() == -1) return -1;

    printf("Validating free inodes blocks\n");
    if (validate_free_inodes() == -1) return -1;

    printf("Validating free data blocks\n");

    printf("File system validated\n");

    free(sprblk);
    free(inode_flags);
    return 0;
}

int main(int argc, char** argv) {
    use_existing_disk = 1;
    disk_name = "MINIFILESYSTEM";

    minithread_system_initialize(fsck, NULL);
    return -1;
}
