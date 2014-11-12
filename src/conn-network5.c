/* 
 *    conn-network test program 5
 *
 *    usage: conn-network5 [<hostname>]
*/

#include "defs.h"
#include "minithread.h"
#include "minisocket.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 100000
#define MAX_CONN 10

int ports[MAX_CONN]; /* port on which we do the communication */
char* hostname;

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

int server(int* arg) {
  int port;
  minisocket_t socket;
  minisocket_error error;

  port = *arg;
  
  socket = minisocket_server_create(port,&error);
  if (socket==NULL){
    printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
    return -1;
  }

  printf("Server connected on port %i\n", port);

  /* close the connection */
  minisocket_close(socket);

  return 0;
}

int client(int* arg) {
  int port;
  network_address_t address;
  minisocket_t socket;
  minisocket_error error;
  
  port = *arg;
  network_translate_hostname(hostname, address);
  
  socket = minisocket_client_create(address, port, &error);
  if (socket==NULL){
    printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
    return -1;
  }

  printf("Client connected on port %i\n", port);
  
  minisocket_close(socket);

  return 0;
}

int spawn_server(int* arg) {
  int i;
  for (i = 0; i < MAX_CONN; i++) {
    minithread_fork(server, &ports[i]);
  }
}

int spawn_client(int* arg) {
  int i;
  for (i = 0; i < MAX_CONN; i++) {
    minithread_fork(client, &ports[i]);
  }
}


int main(int argc, char** argv) {
  int i;
  for (i = 0; i < MAX_CONN; i++) {
    ports[i] = i;
  }

  if (argc > 1) {
    hostname = argv[1];
    minithread_system_initialize(spawn_client, NULL);
  }
  else {
    minithread_system_initialize(spawn_server, NULL);
  }
  return -1;
}

