#ifndef __COMMON_H__
#define __COMMON_H__

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define IP "127.0.0.1"
#define PORT 4444
#define MAX_CLIENTS 5
#define BUF 512

/*
 * @Params: Optional Status ptr
 * @Return: File descriptor for the new socket
 * Create An Endpoint For Communication
 */
int create_socket(void);

/*
 * @Params: Connection Port, IPv4 Address
 * @Return: An Pv4 AF_INET socket struct as defined in <netinet/in.h>
 * Create a socket address structure of family AF_INET
 */
struct sockaddr_in create_socket_address(int port, char *ip_addr);

/*
 * @Params: Socket File Descriptor, ptr to socket attributes
 * @Return: Status Codde according to bind()
 * Assign "Name" To Socket
 */
int bind_connection(int socket, struct sockaddr *sa);

/*
 * @Params: Socket File Descriptor for listener,
 *          Backlog connections to queue.
 * @Return: Status Code according to listen()
 * Listen for incoming connections
 */
int listen_for_connection(int listener_socket, int backlog);

/*
 * @Params: Socket File Descriptor for server,
 *          sockaddr struct for client,
 *          socklen_t: buffer length
 * @Return: Status Code according to accept()
 * Assign "Name" To Socket
 */
int accept_connection_from_client(int server_socket, struct sockaddr *client,
                                  socklen_t *addr_size);

/*
 * @Params: Socket File Descriptor for conecting client,
 *          sockaddr struct for server to connect to,
 * @Return: Status Code according to connect()
 * Connect To Server
 */
int connect_to_server(int client_socket, struct sockaddr *server);

/**
 * @brief Calculate the checksum of file via MD5
 *
 * @param file_path : The path to the file in question
 * @param digest : an unsigned char buffer of length >= 16 bytes
 */
void calculate_md5_hash(const char *file_path, unsigned char *digest);

/**
 * @brief Concatenates the partitions in the cwd into one file
 *
 * @param num_clients = num partitions
 * @param filename : final name of aggregate file
 */
void concat_files(int num_clients, const char *filename);
#endif
