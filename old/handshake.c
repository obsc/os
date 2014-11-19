/* 
 *    Handshake test
 *
 *    spawns two threads, one server and one client and attempts a handshake
 *    and then close
*/

#include "defs.h"
#include "minithread.h"
#include "minisocket.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 100000

int port=80; /* port on which we do the communication */

int receive(int* arg); /* forward declaration */

char* GetErrorDescription(int errorcode){
  switch(errorcode){
  case SOCKET_NOERROR:
    return "No error reported";
    break;

  case SOCKET_NOMOREPORTS:
    return "There are no more ports available";
    break;

  case SOCKET_PORTINUSE:
    return "The port is already in use by the server";
    break;

  case SOCKET_NOSERVER:
    return "No server is listening";
    break;

  case SOCKET_BUSY:
    return "Some other client already connected to the server";
    break;
    
  case SOCKET_SENDERROR:
    return "Sender error";
    break;

  case SOCKET_RECEIVEERROR:
    return "Receiver error";
    break;
    
  default:
    return "Unknown error";
  }
}

int transmit(int* arg) {
  minisocket_t socket;
  minisocket_error error;

  printf("starting transmit\n");

  minithread_fork(receive, NULL);

  printf("continuing transmit\n");

  socket = minisocket_server_create(port,&error);
  if (socket==NULL){
    printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
    return -1;
  }

  printf("server connected\n");

  /* close the connection */
  minisocket_close(socket);

  return 0;
}

int receive(int* arg) {
  network_address_t my_address;
  minisocket_t socket;
  minisocket_error error;

  printf("starting receive\n");

  minithread_yield();

  printf("continuing receive\n");

  network_get_my_address(my_address);

  /* create a network connection to the local machine */
  socket = minisocket_client_create(my_address, port,&error);
  if (socket==NULL){
    printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
    return -1;
  }

  printf("client connected\n");

  minisocket_close(socket);

  return 0;
}

int main(int argc, char** argv) {
  minithread_system_initialize(transmit, NULL);
  return -1;
}
