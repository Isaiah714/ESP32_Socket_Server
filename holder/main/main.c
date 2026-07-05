#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BACKLOG 10
#define BUF_SIZE 1024
#define EXIT_FAILURE 1

void app_main(void) {
  // Creating file descriptors
  int server_fd, client_fd;

  // Creating socket addresses for server and client
  struct sockaddr_in server_addr, client_addr;

  // Getting the size of the address in bytes
  socklen_t client_len = sizeof(client_addr);

  // Creating a buffer
  char buffer[BUF_SIZE];

  // Creating a TCP socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd < 0) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }

  /*
   * Avoid address already in use error
   * This is good practice so do this 
   * everytime. 
   */
  int opt = 1;
  if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt failed");
    exit(EXIT_FAILURE);
  }

  /*
   * Bind to port number and socket 
   * address to addr_in struct obj
   */ 
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);
  bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

  /*
   * Puts the server socket in
   * passive mode waiting for 
   * the client socket to make
   * a connection. BACKLOG is 
   * the maximum queue for
   * client sockets. If a 
   * another connection
   * is attempted when the queue
   * is full, the pending client
   * will recive an error.
   */
  printf("Listening on port%d", PORT);
  if(listen(server_fd, BACKLOG) < 0) {
    perror("listen failed...\n");
    exit(EXIT_FAILURE);
  }

  // Accepts loop
  while(1) {
    /*
     * The server will extract the first
     * connection from the queue of pending
     * connections for the listening socket,
     * sockfd, creates a new connected socket
     * using accept(), the function will return
     * a new file descriptor referrring to the
     * new socket. The server is now ready to
     * transfer data.
     */
    int new_socket_client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if(new_socket_client_fd < 0) {
      perror("failed to create a new socket...\n");
      exit(EXIT_FAILURE);
    }
    printf("Client: %s", inet_ntoa(client_addr.sin_addr));

    int v_read = read(new_socket_client_fd, buffer, BUF_SIZE - 1);
    if(v_read < 0) {
      perror("Failed to read data...\n");
      exit(EXIT_FAILURE);
    }
    printf("%s\n", buffer);
    // Sending a message to client
    const char * message = "Can you hear me?";
    send(new_socket_client_fd, &message, strlen(message), 0);
    printf("Message has been sent to client\n");
    close(new_socket_client_fd);
  }
  close(server_fd);
  printf("Ran program...something?\n");
}
