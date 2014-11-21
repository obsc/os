#include "minithread.h"
#include "minisocket.h"
#include "synch.h"
#include "read.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 100000

char* hostname;
int port = 80; /* port on which we do the communication */
int version;
minisocket_t socket;

int receiver(int *arg) {
  char buffer[BUFFER_SIZE];
  minisocket_error error;
  int len;
  int i;

  while (1) {
    len = minisocket_receive(socket,buffer, BUFFER_SIZE, &error);
    if (len == -1) {
      printf("Receiving error. Code: %d.\n", error);
      minisocket_close(socket);
      return 0;
    } else {
      for (i = 0; i < len; i++) {
        printf("%c", buffer[i]);
      }
    }
  }
  return 0;
}

int sender(int* arg) {
  char buffer[BUFFER_SIZE];
  int bytes_sent;
  int len;
  network_address_t address;
  minisocket_error error;

  if (version == 2) {
    socket = minisocket_server_create(port,&error);
  } else {
    network_translate_hostname(hostname, address);
    /* create a network connection to the remote machine */
    socket = minisocket_client_create(address, port,&error);
  }

  if (socket==NULL){
    printf("Can't create the server. Error code: %d.\n",error);
    return 0;
  }

  minithread_fork(receiver, NULL);
  while (1) {
    len = miniterm_read(buffer, BUFFER_SIZE);

    bytes_sent=0;
    while (bytes_sent!=len){
      int trans_bytes=
        minisocket_send(socket,buffer+bytes_sent,
            len-bytes_sent, &error);
      //printf("Sent %d bytes.\n", trans_bytes);
      if (trans_bytes==-1){
        printf("Sending error. Code: %d.\n", error);
        minisocket_close(socket);
        return 0;
      }
      bytes_sent+=trans_bytes;
    }
  }
  return 0;
}

int main(int argc, char** argv) {

  if (argc > 2) {
      hostname = argv[2];
      port = atoi(argv[1]);
      version = 1;
      minithread_system_initialize(sender, NULL);
  }
  else if (argc > 1) {
      port = atoi(argv[1]);
      version = 2;
      minithread_system_initialize(sender, NULL);
  }
  return -1;
}
