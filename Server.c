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
  int server_socket = 0, client_socket = 0, client_tracker, client_count = 0,
    port = PORT, tracker_port = port+1, tracker_socket = 0;;
  int num_clients = -99999, clients[MAX_CLIENTS], trackers[MAX_CLIENTS];
  size_t num_bytes, bytes_left, bytes_per_partition;
  struct sockaddr_in server, addr, tracker;
  struct stat st;
  char *ip = IP, *file_name, gather[BUF*2], buffer[32], ip_addrs[MAX_CLIENTS][INET_ADDRSTRLEN];
  pthread_t tids[MAX_CLIENTS];
  socklen_t sckln = sizeof(struct sockaddr_in);

  for(int i = 1; i < argc; i += 2)
    if(!strcmp(argv[i],"-i") && argc >= i + 1)
      ip = argv[i + 1];
    else if(!strcmp(argv[i],"-p") && argc >= i + 1)
      port = atoi(argv[i + 1]);
    else if(!strcmp(argv[i],"-n") && argc >= i + 1)
      num_clients = atoi(argv[i + 1]);
    else if(!strcmp(argv[i],"-f") && argc >= i + 1)
      file_name = argv[i + 1];
    else{
      printf("Usage: %s [-n] num_clients [-f] file_name [-i] ip_addr [-p] port\n", argv[0]);
      printf("Default: ip = 127.0.0.0.1, port = 4444\n");
      exit(0);
    }

  if(num_clients <= 0 || num_clients > MAX_CLIENTS){
    fprintf(stderr, "num_clients must be greater than 0 but smaller than %d!\n", MAX_CLIENTS);
    exit(-1);
  }
  
  server_socket = create_socket();
  tracker_socket = create_socket();
  
  server = create_socket_address(port, ip);
  tracker = create_socket_address(tracker_port, ip);
  
  bind_connection(server_socket,(struct sockaddr*)&server);
  bind_connection(tracker_socket, (struct sockaddr*)&tracker);
  
  listen_for_connection(server_socket, MAX_CLIENTS);
  listen_for_connection(tracker_socket, MAX_CLIENTS);
  
  assert(stat(file_name, &st) != -1);
  
  num_bytes = bytes_left = st.st_size;
  bytes_per_partition = num_bytes / num_clients;
  
  /* Accept connections from k clients */
  while((client_count < num_clients) &&
	(client_socket = accept(server_socket, (struct sockaddr *)&server, &sckln)) &&
	(client_tracker = accept(tracker_socket, (struct sockaddr*)&tracker, &sckln))){
    
    Client c;
    c.client_socket = client_socket;
    c.start_pos = num_bytes - bytes_left;
    c.end_pos = (client_count  == num_clients - 1) ? num_bytes : c.start_pos + bytes_per_partition;
    c.file_name = file_name;
    c.client_number = client_count;
    bytes_left -= bytes_per_partition;

    clients[client_count] = client_socket;
    trackers[client_count] = client_tracker;
    
    getpeername(client_socket, (struct sockaddr*)&addr, &sckln);
    printf("[+] Client %d connected from %s\n", client_count, inet_ntoa(addr.sin_addr));
    strcpy(ip_addrs[client_count], inet_ntoa(addr.sin_addr));

    getpeername(tracker_socket, (struct sockaddr*)&addr, &sckln);
    printf("[+] Tracker %d connected from %s\n", client_count, inet_ntoa(addr.sin_addr));
    assert(strcmp(ip_addrs[client_count], inet_ntoa(addr.sin_addr)) == 0);
    
    assert(pthread_create(tids + client_count, NULL, handle_client, &c) == 0);
    ++client_count;
  }

  for(int i = 0; i < num_clients; ++i)
    pthread_join(tids[i], NULL);

  for(int i = 0; i < num_clients; ++i){
    memset(gather, 0, sizeof(gather));
    memset(buffer, 0, sizeof(buffer));
    for(int j = 0; j < num_clients; ++j){
      if(i == j) continue;
      sprintf(buffer, "%d:%s,", j, ip_addrs[j]);
      strcat(gather, buffer);
    }
    send(trackers[i], gather, sizeof(gather), 0);
    close(clients[i]);
    close(trackers[i]);
  }

  printf("[+] Transfer complete. Closing Sockets.\n");
  close(server_socket);
  close(tracker_socket);
  return 0;
}

void* handle_client(void* args){
  Client* c = (Client*) args;
  char buffer[BUF];
  const int bytes_to_send = c->end_pos - c->start_pos;
  off_t offset = c->start_pos;
  int sent_bytes, fd, bytes_remaining = bytes_to_send;
  
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "You are client #%d. Sending (%d) bytes.", c->client_number, bytes_to_send);
  assert(send(c->client_socket, buffer, BUF, 0) > 0);
  assert((fd = open(c->file_name, O_RDONLY)) != 1);
  
  while((sent_bytes = sendfile(c->client_socket, fd, &offset, BUF)) > 0 &&
	(bytes_remaining > 0)){
    bytes_remaining -= sent_bytes;
    
    fprintf(stdout, "[+] %lf%c: ", 100.0 * (1 - (double)bytes_remaining/bytes_to_send), '%');
    fprintf(stdout, "Sent %d bytes to client %d\n", sent_bytes, c->client_number);
  }

  /*TODO Send MD5 checksum of the portion of the file*/
  
  pthread_exit(NULL);
}
