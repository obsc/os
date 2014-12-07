#include "minithread.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disk.h"

#include "minifile.h"

int mkfs(int *arg) {
    //make datastructs
    return -1;
}

int main(int argc, char** argv) {
    use_existing_disk = 0;
    disk_name = "MINIFILESYSTEM";
    disk_flags = DISK_READWRITE;
    disk_size = 1000;

    minithread_system_initialize(mkfs, NULL);
    return -1;
}
