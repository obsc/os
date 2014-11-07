/* network test for send and receive
 * sends two messages while three threads
 * are waiting. Test for correct order:
 * (a,b,c waiting in order, a,b get messages)
 *
 *    USAGE: ./network_send_test2 <port>
 * 
 */

#include "defs.h"
#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_COUNT 2

char* hostname;

int
receive(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    int cur_id;
    miniport_t port;
    miniport_t from;

    cur_id = *arg;

    port = miniport_create_unbound(1);

    length = BUFFER_SIZE;
    printf("I am receiver %i\n", cur_id);
    minimsg_receive(port, &from, buffer, &length);
    printf("Receiver %i, %s", cur_id, buffer);
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

    network_get_my_address(addr);
    port = miniport_create_unbound(0);
    dest = miniport_create_bound(addr, 1);

    for (i=0; i<MAX_COUNT; i++) {
        printf("Sending packet %d.\n", i+1);
        sprintf(buffer, "Message number %d.\n", i+1);
        length = strlen(buffer) + 1;
        minimsg_send(port, dest, buffer, length);
    }

    return 0;
}

int
spawner(int* arg) {
    int a,b,c;
    a = 1;
    b = 2;
    c = 3;

    minithread_fork(receive, &a);
    minithread_fork(receive, &b);
    minithread_fork(receive, &c);
    minithread_sleep_with_timeout(1000);
    minithread_fork(transmit, NULL);
    return 0;
}

int
main(int argc, char** argv) {
    short fromport;
    fromport = atoi(argv[1]);
    network_udp_ports(fromport,fromport);
    minithread_system_initialize(spawner, NULL);
    return -1;
}

