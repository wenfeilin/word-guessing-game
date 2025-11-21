#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#include "message.h"
#include "socket.h"

int main()
{
  // Open a server socket
  unsigned short port = 0;
  int server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1)
  {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // Start listening for connections, with a maximum of one queued connection
  if (listen(server_socket_fd, 1))
  {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %u\n", port);

  // Wait for a client to connect
  while (true) {
    int client_socket_fd = server_socket_accept(server_socket_fd);
    if (client_socket_fd == -1)
    {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    printf("Client connected!\n");

    // Send a message to the client

    // int rc = send_message(client_socket_fd, "Hello client!");
    // if (rc == -1) {
    //   perror("Failed to send message to client");
    //   exit(EXIT_FAILURE);
    // }

    // Read a message from the client
    char *message = receive_message(client_socket_fd);

    while (strcmp(message, "quit") != 0)
    {
      if (message == NULL)
      {
        perror("Failed to read message from client");
        exit(EXIT_FAILURE);
      }

      // Capitalize message
      for (char *s = message; *s != '\0'; s++)
      {
        *s = (char)toupper((int)*s);
      }

      int rc = send_message(client_socket_fd, message);
      if (rc == -1)
      {
        perror("Failed to send message to client");
        exit(EXIT_FAILURE);
      }
      message = receive_message(client_socket_fd);
    }

    // Print the message
    printf("Client sent: %s\n", message);

    // Free the message string
    free(message);

    close(client_socket_fd);
  }


  // Close sockets
  close(server_socket_fd);

  return 0;
}
