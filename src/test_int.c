/* test_int.c

*/

#include "minithread.h"
#include "synch.h"

#include <stdio.h>
#include <stdlib.h>

#define delay() for(i = 0; i < 1000000; i++);

int a;
int b;

int thread2(int* arg) {
    int i;
    while(1) {
        delay();
        a++;
        printf("a: %i\n", a);
    }

    return 0;
}

int thread1(int* arg) {
    int i;
    minithread_fork(thread2, NULL);

    while(1) {
        delay();
        b++;
        printf("b: %i\n", b);
    }

    return 0;
}

int
main(int argc, char *argv[]) {
    a = 0;
    b = 0;
    minithread_system_initialize(thread1, NULL);
    return -1;
}
