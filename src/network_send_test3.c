/* network test for send
 * 1. send a message to a port 
 *    and then delete the port and recreate as well as listen on it
 *    behavior: message should be dropped
 */

#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define BUFFER_SIZE 256


miniport_t receive_port;
miniport_t send_port;

char text[] = "Hello, world!\n";
int textlen = 14;

int
thread(int* arg) {
    char buffer[BUFFER_SIZE];
    int length = BUFFER_SIZE;
    miniport_t from;
    network_address_t my_address;

    network_get_my_address(my_address);
    receive_port = miniport_create_unbound(0);
    send_port = miniport_create_bound(my_address, 0);

    minimsg_send(receive_port, send_port, text, textlen);
    miniport_destroy(receive_port);
    receive_port = miniport_create_unbound(0);
    minimsg_receive(receive_port, &from, buffer, &length);
    printf("%s", buffer); //should not print

    return 0;
}

int
main(int argc, char** argv) {
    short fromport;
    fromport = atoi(argv[1]);
    network_udp_ports(fromport,fromport);
    textlen = strlen(text) + 1;
    minithread_system_initialize(thread, NULL);
    return -1;
}

