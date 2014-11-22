#include "minithread.h"
#include "minisocket.h"
#include "synch.h"
#include "read.h"
#include "read_private.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 100000

char* hostname;
int port = 80; /* port on which we do the communication */
int version_num;
minisocket_t socket_glob;

int receiver(int *arg) {
  char buffer[BUFFER_SIZE];
  minisocket_error error;
  int len;

  while (1) {
    len = minisocket_receive(socket_glob,buffer, BUFFER_SIZE, &error);
    if (len == -1) {
      printf("Receiving error. Code: %d.\n", error);
      minisocket_close(socket_glob);
      return 0;
    } else {
      printf("%s", buffer);
    }
  }
  return 0;
}

int sender(int* arg) {
  char buffer[BUFFER_SIZE];
  int bytes_sent;
  int len;
  int trans_bytes;
  network_address_t address;
  minisocket_error error;

  if (version_num == 2) {
    socket_glob = minisocket_server_create(port,&error);
  } else {
    network_translate_hostname(hostname, address);
    /* create a network connection to the remote machine */
    socket_glob = minisocket_client_create(address, port,&error);
  }

  if (socket_glob==NULL){
    printf("Can't create the server. Error code: %d.\n",error);
    return 0;
  }

  minithread_fork(receiver, NULL);
  while (1) {
    len = miniterm_read(buffer, BUFFER_SIZE);

    bytes_sent=0;
    while (bytes_sent!=len){
      trans_bytes=
        minisocket_send(socket_glob,buffer+bytes_sent,
            len-bytes_sent, &error);
      //printf("Sent %d bytes.\n", trans_bytes);
      if (trans_bytes==-1){
        printf("Sending error. Code: %d.\n", error);
        minisocket_close(socket_glob);
        return 0;
      }
      bytes_sent+=trans_bytes;
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  miniterm_initialize();

  if (argc > 1) {
      hostname = argv[1];
      version_num = 1;
      minithread_system_initialize(sender, NULL);
  }
  else {
      version_num = 2;
      minithread_system_initialize(sender, NULL);
  }
  return -1;
}
