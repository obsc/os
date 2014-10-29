/* network load test
*/

#include "defs.h"
#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_COUNT 1000

char* hostname;

int
receive(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    miniport_t port;
    miniport_t from;

    port = miniport_create_unbound(1);

    length = BUFFER_SIZE;
    minimsg_receive(port, &from, buffer, &length);
    printf("%s", buffer);
    miniport_destroy(from);

    return 0;
}

int
transmit(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    int i;
    network_address_t addr;
    miniport_t port;
    miniport_t dest;

    AbortOnCondition(network_translate_hostname(hostname, addr) < 0,
                     "Could not resolve hostname, exiting.");

    port = miniport_create_unbound(0);
    dest = miniport_create_bound(addr, 1);

    for (i=0; i<MAX_COUNT; i++) {
        printf("Sending packet %d.\n", i);
        sprintf(buffer, "Count is %d.\n", i);
        length = strlen(buffer) + 1;

        minithread_sleep_with_timeout(1);

        minimsg_send(port, dest, buffer, length);
    }

    return 0;
}

int
spawner_receive (int *arg) {
    int i;
    for (i=0; i<MAX_COUNT; i++) {
        minithread_fork(receive, NULL);
    }
    return 0;
}

int
main(int argc, char** argv) {
    short fromport, toport;
    fromport = atoi(argv[1]);
    toport = atoi(argv[2]);
    network_udp_ports(fromport,toport);

    if (argc > 3) {
        hostname = argv[3];
        minithread_system_initialize(transmit, NULL);
    }
    else {
        minithread_system_initialize(spawner_receive, NULL);
    }

    return -1;
}

