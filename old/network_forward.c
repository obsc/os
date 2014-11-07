/* network forwarding program
    
    fowards all incoming traffic to another machine.

    USAGE: ./network_forward <souceport> <destport> <hostname>

    sourceport = udp port to listen on.
    destport   = udp port to send to.
    hostname   = hostname to forward all traffic to


    This best interfaces with network5:
    start a listening server: ./network5 <a> <b>
    start a forwarding server: ./network_forwarding <c> <a> <listening hostname>
    start a sending client: ./network5 <d> <c> <forwarding hostname>
*/

#include "defs.h"
#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256

char* hostname;

int
forward(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    network_address_t addr;
    miniport_t port;
    miniport_t dest;
    miniport_t from;

    AbortOnCondition(network_translate_hostname(hostname, addr) < 0,
                     "Could not resolve hostname, exiting.");

    port = miniport_create_unbound(1);
    dest = miniport_create_bound(addr, 1);

    while (1) {
        length = BUFFER_SIZE;
        minimsg_receive(port, &from, buffer, &length);
        printf("%s", buffer);
        printf("Forwarding packet.\n");
        length = strlen(buffer) + 1;
        minimsg_send(port, dest, buffer, length);
        miniport_destroy(from);
    }

    return 0;
}

int
main(int argc, char** argv) {

    short fromport, toport;
    fromport = atoi(argv[1]);
    toport = atoi(argv[2]);
    hostname = argv[3];

    network_udp_ports(fromport,toport); 

    minithread_system_initialize(forward, NULL);

    return -1;
}

