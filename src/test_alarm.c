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

int thread1(int* arg) {
    register_alarm(1000, func, NULL);
    register_alarm(2000, func, NULL);
    register_alarm(3000, func, NULL);
    minithread_sleep_with_timeout(5000);
    register_alarm(4000, func, NULL);
    register_alarm(5000, func, NULL);

    return 0;
}

int
main(int argc, char *argv[]) {
    a = 0;
    minithread_system_initialize(thread1, NULL);
    return -1;
}
