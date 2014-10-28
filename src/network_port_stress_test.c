/* network stress test for ports
 * creates and deletes ports to test overflow
 */

#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>



int
thread(int* arg) {
    network_address_t my_address;
    int i;
    queue_t listen_ports;
    queue_t send_ports;
    miniport_t listen_port;
    miniport_t send_port;
    void * data;

    listen_ports = queue_new();
    send_ports = queue_new();
    network_get_my_address(my_address);

    for (i = 0; i < 2*NUMPORTS; i++) {
        listen_port = miniport_create_unbound(i);
        send_port = miniport_create_bound(my_address, i);
        if (!listen_port) {
            queue_append(listen_ports, listen_port);
        }
        if (!send_port) {
            queue_append(send_ports, send_port);
        }
    }

    while (queue_dequeue(listen_ports, &data) == 0) {
        miniport_destroy((miniport_t) data);
    }
    while (queue_dequeue(send_ports, &data) == 0) {
        miniport_destroy((miniport_t) data);
    }

    queue_free(listen_ports);
    queue_free(send_ports);

    printf("all finished");
    


    return 0;
}

int
main(int argc, char** argv) { 
    minithread_system_initialize(thread, NULL);
    return -1;
}


