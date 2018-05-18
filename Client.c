#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "Connect/Connect.h"

typedef struct peers{
  char** peers; 
  int num_peers;       
  int peer_index;
}Peers;

typedef struct gather_args{
  Peers* peers;
  int sending;
}GatherArgs;

void* gather(void*);

int main(int argc, char* argv[]){
  
  int client_socket, tracker_socket,  port = PORT, i, 
    tracker_port = port + 1, client_number, bytes_remaining,
    bytes_rcvd, num_peers = 0, client_index, peer_indicies[MAX_CLIENTS];
  size_t bytes_to_receive;
  struct sockaddr_in server_addr, tracker_addr;
  char buffer[BUF], peer[BUF*2], *ip = IP, *ptr,
    peers[MAX_CLIENTS][INET_ADDRSTRLEN], ipa[INET_ADDRSTRLEN];
  FILE *fp;
  
  for(int i = 1; i < argc; i += 2)
    if(!strcmp(argv[i],"-i") && argc >= i + 1)
      ip = argv[i + 1];
    else if(!strcmp(argv[i],"-p") && argc >= i + 1)
      port = atoi(argv[i + 1]);
    else{
      printf("Usage: %s [-i] ip_addr [-p] port\n", argv[0]);
      printf("Default: ip = 127.0.0.0.1, port = 4444\n");
      exit(0);
    }
  
  client_socket = create_socket();
  server_addr = create_socket_address(port, ip);
  connect_to_server(client_socket,(struct sockaddr*)&server_addr);

  tracker_socket = create_socket();
  tracker_addr = create_socket_address(tracker_port, ip);
  connect_to_server(tracker_socket,(struct sockaddr*)&tracker_addr);

  assert(recv(client_socket, buffer, BUF, 0) > 0);
  
  ptr = strtok(buffer, "#");
  ptr = strtok(NULL,  "#");
  ptr = strtok(ptr, ".");
  assert(ptr != NULL);
  client_number = atoi(ptr);

  ptr = strtok(NULL, ".");
  ptr = strtok(ptr, "(");
  ptr = strtok(NULL, "(");
  ptr = strtok(ptr, ")");
  assert(ptr != NULL);
  bytes_to_receive = bytes_remaining = atoi(ptr);

  printf("[+] You are client %d. Receiving %lu bytes.\n", client_number, bytes_to_receive);

  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "%d", client_number);
  
  assert((fp = fopen(buffer, "w")) != NULL);

  while((bytes_rcvd = recv(client_socket, buffer, BUF, 0)) > 0 &&
	bytes_remaining > 0){
    bytes_remaining -= bytes_rcvd;
    fwrite(buffer, sizeof(char), bytes_remaining < 0 ? abs(bytes_remaining) : bytes_rcvd, fp);
    printf("[+] %lf%c: ", 100.0 * (1 - (double)bytes_remaining/bytes_to_receive), '%');
    printf("Received %lu bytes from server\n", bytes_to_receive - bytes_remaining);
  }

  memset(peer, 0, sizeof(peer));
  assert(recv(tracker_socket, peer, sizeof(peer), 0) > 0);
  ptr = strtok(peer, ",");
  assert(ptr != NULL);

  do{
    memset(buffer, 0, sizeof(buffer));
    memset(ipa, 0, sizeof(ipa));
    memset(peers[num_peers], 0, sizeof(peers[num_peers]));
    i = 0;
    strcpy(buffer, ptr);
    while(*(buffer + i++) != ':');
    buffer[i - 1] = 0;
    client_index = atoi(buffer);
    strcpy(peers[num_peers], buffer + i);
    peer_indicies[num_peers++] = client_index;
  }while((ptr = strtok(NULL, ",")) != NULL);

  printf("Connected Peers...\n");
  for(i = 0; i < num_peers; ++i)
    printf("Client = %d, IP = %s\n", peer_indicies[i], peers[i]);
    
  fclose(fp);
  close(client_socket);
  close(tracker_socket);
  return 0;
}
