#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "message.h"
#include "socket.h"

void* user_thread (void* args) {

  int* socket = (int *) args;

  int socket_fd = *socket;

  // Get a message from the user
  char* line = NULL;
  size_t size = 0;
  
  while (getline(&line, &size, stdin)) {
    line[strlen(line) - 1] = '\0';    
    int rc = send_message(socket_fd, line);
    if (rc == -1) {
      perror("Failed to send message to server");
      exit(EXIT_FAILURE);
    }
    if (strcmp(line, "quit") == 0) break;
  }

  return NULL;
}

void* server_thread (void* args) {

  int* socket = (int *) args;

  int socket_fd = *socket;
  // Read a message from the server
  char* line = NULL;
  size_t size = 0;
  
  while (true) {
    // Receive a message from the server
    char* message = receive_message(socket_fd);
    if (message == NULL) {
      perror("Failed to read message from server");
      exit(EXIT_FAILURE);
    }
    printf("Server: %s\n", message);
    // Free the message
    free(message);
  }
}

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <server name> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Read command line arguments
  char* server_name = argv[1];
  unsigned short port = atoi(argv[2]);

  // Connect to the server
  int socket_fd = socket_connect(server_name, port);
  if (socket_fd == -1) {
    perror("Failed to connect");
    exit(EXIT_FAILURE);
  }
  
  pthread_t input_thread;
  pthread_t output_thread;
  pthread_create(&input_thread, NULL, user_thread, &socket_fd);
  pthread_create(&output_thread, NULL, server_thread, &socket_fd);

  pthread_join(input_thread, NULL);
  pthread_join(output_thread, NULL);

  // Close socket
  close(socket_fd);

  return 0;
}
