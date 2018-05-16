#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>

#include "Connect/Connect.h"

typedef struct client{
  int client_socket;
  int client_number;
  int start_pos;
  int end_pos;
  const char* file_name;
}Client;

void* handle_client(void*);

int main(int argc, char* argv[]){
  int server_socket, client_socket, clients[MAX_CLIENTS], client_count = 0, port = PORT;
  int num_clients = -99999;
  size_t num_bytes, bytes_left, bytes_per_partition;
  struct sockaddr_in server;
  struct stat st;
  char *ip = IP, *file_name = NULL;
  pthread_t tids[MAX_CLIENTS];
  socklen_t sckln = sizeof(struct sockaddr_in);
  
  for(int i = 1; i < argc; i += 2){
    if(!strcmp(argv[i],"-i") && argc >= i + 1)
      ip = argv[i + 1];
    else if(!strcmp(argv[i],"-p") && argc >= i + 1)
      port = atoi(argv[i + 1]);
    else if(!strcmp(argv[i],"-n") && argc >= i + 1)
      num_clients = atoi(argv[i + 1]);
    else if(!strcmp(argv[i],"-f") && argc >= i + 1)
      port = atoi(argv[i + 1]);
    else{
      printf("Usage: %s [-n] num_clients [-f] file_name [-i] ip_addr [-p] port\n", argv[0]);
      printf("Default: ip = 127.0.0.0.1, port = 4444\n");
      exit(0);
    }
  }

  if(file_name == NULL || access(file_name, F_OK) == -1){
    fprintf(stderr, "Please provide a valid file path!\n");
    exit(-1);
  }else if(num_clients <= 0 || num_clients > MAX_CLIENTS){
    fprintf(stderr, "num_clients must be greater than 0 but smaller than %d!\n", MAX_CLIENTS);
    exit(-1);
  }
  
  memset(&server_socket,0x0,sizeof(server_socket));

  server_socket = create_socket();
  server = create_socket_address(port, ip);
  bind_connection(server_socket,(struct sockaddr*)&server);
  listen_for_connection(server_socket, MAX_CLIENTS);


  /* Accept connections from k clients */
  while((client_socket = accept(server_socket,(struct sockaddr *)&server,&sckln))
	&& client_count < num_clients){
    clients[client_count] = client_socket;
    printf("[+] Client %d connected! \n", client_count++);
  }
  
  assert(stat(file_name, &st) != -1);

  num_bytes = bytes_left = st.st_size;
  bytes_per_partition = num_bytes / num_clients;
  
  for(int i = 0; i < num_clients; ++i){
    Client c;
    c.client_socket = clients[i];
    c.start_pos = num_bytes - bytes_left;
    c.end_pos = (i == num_clients - 1) ? num_bytes : c.start_pos + bytes_per_partition;
    c.file_name = file_name;
    c.client_number = i;
    bytes_left -= bytes_per_partition;

    assert(pthread_create(tids + i, NULL, handle_client, &c) < 0);
  }
  
  for(int i = 0; i < num_clients; ++i)
    pthread_join(tids[i], NULL);
  
  printf("[-] Transfer complete. Closing Server Socket.\n");
  close(server_socket);
  return 0;
}

void* handle_client(void* args){
  Client* c = (Client*) args;
  char buffer[BUF];
  const int bytes_to_send = c->end_pos - c->start_pos;
  off_t offset = c->start_pos;
  int sent_bytes, fd, bytes_remaining = bytes_to_send;
  
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "Sending: %d bytes.", bytes_to_send);
  assert(send(c->client_socket, buffer, BUF, 0) > 0);

  assert((fd = open(c->file_name, O_RDONLY)) != 1);
  
  while((bytes_remaining > 0) &&
	(sent_bytes = sendfile(c->client_socket, fd, &offset, BUF)) > 0){
    fprintf(stdout, "[+] %lf%c: ", 100.0 * (1 - (double)bytes_remaining/bytes_to_send), '%');
    fprintf(stdout, "Sent %d bytes to client %d\n", sent_bytes, c->client_number);
    bytes_remaining -= sent_bytes;
  }

  /*TODO Send MD5 checksum of the portion of the file*/
  
  pthread_exit(NULL);
}
