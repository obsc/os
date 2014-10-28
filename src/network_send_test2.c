/* network test for send and receive
 * sends two messages while three threads
 * are waiting. Test for correct order:
 * (a,b,c waiting in order, a,b get messages)
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
int receiver_num;

int
receive(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    int i;
    miniport_t port;
    miniport_t from;

    port = miniport_create_unbound(1);

    for (i=0; i<MAX_COUNT; i++) {
        length = BUFFER_SIZE;
        printf("I am receiver %i\n", receiver_num);
        receiver_num +=1;
        minimsg_receive(port, &from, buffer, &length);
        printf("%s", buffer);
        miniport_destroy(from);
    }

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
        printf("Sending packet %d.\n", i+1);
        sprintf(buffer, "Message number %d.\n", i+1);
        length = strlen(buffer) + 1;
        minimsg_send(port, dest, buffer, length);
    }

    return 0;
}

int
spawner(int* arg) {
    minithread_fork(receiver, NULL);
    minithread_fork(receiver, NULL);
    minithread_fork(receiver, NULL);
    minithread_yield();
    minithread_fork(transmit, NULL);
}

int
main(int argc, char** argv) {
    short fromport, toport;
    fromport = atoi(argv[1]);
    toport = atoi(argv[2]);
    network_udp_ports(fromport,toport); 
    hostname = argv[3];
    receiver_num = 1;
    minithread_system_initialize(spawner, NULL);
    return -1;
}

