/* test_alarm.c

*/

#include "minithread.h"
#include "alarm.h"

#include <stdio.h>
#include <stdlib.h>

int a;

void func() {
    printf("HELLO\n");
}

int thread2(int* arg) {
    register_alarm(1000, func, NULL); // 1
    register_alarm(4000, func, NULL); // 4
    minithread_sleep_with_timeout(4000);
    register_alarm(1000, func, NULL); // 5
    register_alarm(3000, func, NULL); // 7

    return 0;
}

int thread1(int* arg) {
    minithread_fork(thread2, NULL);

    register_alarm(10000, func, NULL); // 10
    register_alarm(3000, func, NULL); // 3
    register_alarm(2000, func, NULL); // 2
    minithread_sleep_with_timeout(5000);
    register_alarm(3000, func, NULL); // 8
    register_alarm(1000, func, NULL); // 6
    minithread_sleep_with_timeout(1000);
    register_alarm(3000, func, NULL); // 9

    return 0;
}

int
main(int argc, char *argv[]) {
    a = 0;
    minithread_system_initialize(thread1, NULL);
    return -1;
}
