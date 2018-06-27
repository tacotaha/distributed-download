#include <assert.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#include "Common/Common.h"

typedef struct peer {
  char ip[INET_ADDRSTRLEN];
  int index;
} Peer;

typedef struct peers Peers;
struct peers {
  Peer *peers;
  int num_peers;
  int my_index;
};

typedef struct gather_args {
  Peers *peers;
  int client_index;
  int outfd;
  int infd;
} GatherArgs;

void *send_thread(void *);
void *recv_thread(void *);

int main(int argc, char *argv[]) {
  int client_socket, tracker_socket, inbound_sockets[MAX_CLIENTS],
      outbound_sockets[MAX_CLIENTS], outfds[MAX_CLIENTS],
      yes = 1, status = 0, port = PORT, i, tracker_port = port + 1,
      client_number, bytes_remaining, bytes_rcvd, num_peers = 0, client_index,
      thread_index = 0;
  socklen_t sckln = sizeof(struct sockaddr_in);
  size_t bytes_to_receive;
  struct sockaddr_in server_addr, tracker_addr, inbound_addrs[MAX_CLIENTS],
      outbound_addrs[MAX_CLIENTS];
  char buffer[BUF], peer[BUF * 2], filename[BUF], *ip = IP, *ptr;
  unsigned char incoming_checksum[MD5_DIGEST_LENGTH],
      local_checksum[MD5_DIGEST_LENGTH];
  Peer peers[MAX_CLIENTS], me;
  GatherArgs thread_args[MAX_CLIENTS];
  Peers p;
  pthread_t tids[2 * MAX_CLIENTS - 2];
  FILE *fp;

  for (int i = 1; i < argc; i += 2)
    if (!strcmp(argv[i], "-i") && argc >= i + 1)
      ip = argv[i + 1];
    else if (!strcmp(argv[i], "-p") && argc >= i + 1)
      port = atoi(argv[i + 1]);
    else {
      printf("Usage: %s [-i] ip_addr [-p] port\n", argv[0]);
      printf("Default: ip = 127.0.0.0.1, port = 4444\n");
      exit(0);
    }

  client_socket = create_socket();
  server_addr = create_socket_address(port, ip);
  connect_to_server(client_socket, (struct sockaddr *)&server_addr);

  tracker_socket = create_socket();
  tracker_addr = create_socket_address(tracker_port, ip);
  connect_to_server(tracker_socket, (struct sockaddr *)&tracker_addr);

  printf("[+] Waiting on peers...\n");
  memset(buffer, 0, sizeof(buffer));
  assert(recv(client_socket, buffer, BUF, 0) > 0);

  ptr = strtok(buffer, "#");
  ptr = strtok(NULL, "#");
  ptr = strtok(ptr, ".");
  assert(ptr != NULL);
  client_number = atoi(ptr);

  ptr = strtok(NULL, ".");
  ptr = strtok(ptr, "(");
  ptr = strtok(NULL, "(");
  ptr = strtok(ptr, ")");
  assert(ptr != NULL);
  bytes_to_receive = bytes_remaining = atoi(ptr);

  printf("[+] You are client %d. Receiving %lu bytes from the server.\n",
         client_number, bytes_to_receive);
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "%d", client_number);
  assert((fp = fopen(buffer, "w")) != NULL);

  while ((bytes_rcvd = recv(client_socket, buffer, BUF, 0)) > 0 &&
         bytes_remaining > 0) {
    bytes_remaining -= bytes_rcvd;
    fwrite(buffer, sizeof(char),
           bytes_remaining < 0 ? abs(bytes_remaining) : bytes_rcvd, fp);
    printf("[+] %lf%c: ",
           100.0 * (1 - (double)bytes_remaining / bytes_to_receive), '%');
    printf("Received %lu bytes from server\n",
           bytes_to_receive - bytes_remaining);
  }

  fclose(fp);
  memset(peer, 0, sizeof(peer));
  assert(recv(tracker_socket, peer, BUF * 2, 0) > 0);

  assert(recv(tracker_socket, incoming_checksum, MD5_DIGEST_LENGTH, 0) > 0);
  assert((bytes_rcvd = recv(tracker_socket, filename, BUF, 0)) > 0);
  filename[bytes_rcvd] = 0;

  if (strcmp(peer, "")) {
    ptr = strtok(peer, ",");
    assert(ptr != NULL);

    close(client_socket);
    close(tracker_socket);

    strcpy(me.ip, "0.0.0.0");
    me.index = client_number;
    peers[client_number] = me;

    do {
      if (num_peers == client_number) ++num_peers;
      memset(buffer, 0, sizeof(buffer));
      Peer p;
      i = 0;
      strcpy(buffer, ptr);
      while (*(buffer + i++) != ':');
      buffer[i - 1] = 0;
      client_index = atoi(buffer);
      strcpy(p.ip, buffer + i);
      p.index = client_index;
      peers[num_peers++] = p;
    } while ((ptr = strtok(NULL, ",")) != NULL);

    num_peers += (num_peers <= client_number);

    printf("[+] Spawning out-bound connections...\n");
    for (int i = 0; i < num_peers; ++i)
      if (i == client_number) {
        for (int j = 0; j < num_peers; ++j) {
          if (j == i) continue;
          printf("[+] Starting out-bound connection to client %d\n", j);
          outbound_sockets[j] = create_socket();
          outbound_addrs[j] =
              create_socket_address(PORT + j + 1 + client_number, "0.0.0.0");
          printf("[+] Listening on port %d\n", PORT + j + 1 + client_number);
          assert(bind_connection(outbound_sockets[j],
                                 (struct sockaddr *)outbound_addrs + j) == 0);
          listen_for_connection(outbound_sockets[j], MAX_CLIENTS);
          outfds[j] = accept(outbound_sockets[j],
                             (struct sockaddr *)outbound_addrs + j, &sckln);
          printf("[+] Connected to client %d\n", j);
        }
      } else {
        printf("[+] Starting inbound connection to client %d\n", i);
        inbound_sockets[i] = create_socket();
        status = 0;
        do {
          close(inbound_sockets[i]);
          inbound_sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
          setsockopt(inbound_sockets[i], SOL_SOCKET, SO_REUSEADDR, &yes,
                     sizeof(yes));
          inbound_addrs[i] =
              create_socket_address(PORT + i + 1 + client_number, peers[i].ip);
          printf("[+] Connecting to port %d at ip : %s\n",
                 PORT + i + 1 + client_number, peers[i].ip);
          status =
              connect(inbound_sockets[i], (struct sockaddr *)inbound_addrs + i,
                      sizeof(inbound_addrs[0]));
        } while (status < 0);
        printf("[+] Connected to client %d\n", i);
      }

    p.peers = peers;
    p.num_peers = num_peers;
    p.my_index = client_number;

    for (i = 0; i < num_peers; ++i) {
      if (i == client_number) continue;
      printf("Client = %d, IP = %s\n", peers[i].index, peers[i].ip);
      GatherArgs *ga = thread_args + i;
      assert(ga != NULL);
      ga->peers = &p;
      ga->client_index = peers[i].index;
      ga->outfd = outfds[i];
      ga->infd = inbound_sockets[i];
      assert(pthread_create(tids + thread_index++, NULL, send_thread,
                            thread_args + i) == 0);
      printf("[+] Spawned outbound thread to client %d.\n", ga->client_index);
      assert(pthread_create(tids + thread_index++, NULL, recv_thread,
                            thread_args + i) == 0);
      printf("[+] Spawned inbound thread from client %d.\n", ga->client_index);
    }

    for (int i = 0; i < num_peers * 2 - 2; ++i) pthread_join(tids[i], NULL);

    printf("[+] Merging files...\n");
    concat_files(num_peers, filename);
  } else
    assert(rename("0", filename) == 0);

  calculate_md5_hash(filename, local_checksum);
  printf("[+] Successfully downloaded file %s\n", filename);
  printf("[+] Performing file integrity checks...\n");

  printf("[+] Incoming checksum: ");
  for (size_t i = 0; i < MD5_DIGEST_LENGTH; ++i)
    printf("%02x", incoming_checksum[i]);

  printf("\n[+] Local checksum:    ");
  for (size_t i = 0; i < MD5_DIGEST_LENGTH; ++i)
    printf("%02x", local_checksum[i]);
  printf("\n");

  for (size_t i = 0; i < MD5_DIGEST_LENGTH; ++i) {
    if (local_checksum[i] != incoming_checksum[i]) {
      fprintf(stderr,
              "[-] File checksums don't match, possible data corruption "
              "detected!\n");
      exit(1);
    }
  }
  printf("[+] Success! File transfer complete.\n");

  return 0;
}

void *send_thread(void *args) {
  GatherArgs *ga = (GatherArgs *)args;
  Peers *p = (Peers *)ga->peers;
  char buffer[BUF];
  int bytes_sent, bytes_to_send, bytes_remaining, fd;
  off_t offset = 0;
  struct stat st;

  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "%d", p->my_index);
  assert(stat(buffer, &st) != -1);
  bytes_to_send = bytes_remaining = st.st_size;
  assert((fd = open(buffer, O_RDONLY)) != 1);

  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "I am client #%d. Sending (%d) bytes.", p->my_index,
          bytes_to_send);
  assert(send(ga->outfd, buffer, BUF, 0) > 0);

  printf("[+] SEND: Sending %d bytes to client %d\n", bytes_to_send,
         ga->client_index);
  while ((bytes_sent = sendfile(
              ga->outfd, fd, &offset,
              (bytes_remaining - BUF < 0) ? bytes_remaining : BUF)) > 0 &&
         (bytes_remaining > 0)) {
    bytes_remaining -= bytes_sent;
    printf("[+] SEND: %lf%c: ",
           100.0 * (1 - (double)bytes_remaining / bytes_to_send), '%');
    printf("[+] Sent %d bytes to client %d\n", bytes_sent, ga->client_index);
  }

  close(ga->outfd);
  pthread_exit(NULL);
}

void *recv_thread(void *args) {
  GatherArgs *ga = (GatherArgs *)args;
  Peers *p = (Peers *)ga->peers;
  char buffer[BUF], *ptr;
  int bytes_to_receive, client_number, bytes_remaining, bytes_rcvd;
  FILE *fp;

  assert(recv(ga->infd, buffer, BUF, 0) > 0);

  ptr = strtok(buffer, "#");
  ptr = strtok(NULL, "#");
  ptr = strtok(ptr, ".");
  assert(ptr != NULL);
  client_number = atoi(ptr);

  ptr = strtok(NULL, ".");
  ptr = strtok(ptr, "(");
  ptr = strtok(NULL, "(");
  ptr = strtok(ptr, ")");
  assert(ptr != NULL);
  bytes_to_receive = bytes_remaining = atoi(ptr);

  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "%d", client_number);
  assert((fp = fopen(buffer, "wb")) != NULL);

  while ((bytes_rcvd = recv(ga->infd, buffer, BUF, 0)) > 0 &&
         bytes_remaining > 0) {
    bytes_remaining -= bytes_rcvd;
    fwrite(buffer, sizeof(char),
           bytes_remaining < 0 ? abs(bytes_remaining) : bytes_rcvd, fp);
    printf("[+] RECV: %lf%c: ",
           100.0 * (1 - (double)bytes_remaining / bytes_to_receive), '%');
    printf("Received %d bytes from client %d\n",
           bytes_to_receive - bytes_remaining,
           p->peers[ga->client_index].index);
  }

  fclose(fp);
  close(ga->infd);
  pthread_exit(NULL);
}
