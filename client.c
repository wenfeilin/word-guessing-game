#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "message.h"
#include "socket.h"

// Keep the username in a global so we can access it
const char *username;

void* user_thread (void* args) {

  int* socket = (int *) args;

  int socket_fd = *socket;

  // Get a message from the user
  char* line = NULL;
  size_t size = 0;
  
  while (getline(&line, &size, stdin)) {
    line[strlen(line) - 1] = '\0';

    if (strcmp(line, "quit") == 0) break;

    user_info_t* user_info = malloc(sizeof(user_info_t));
    user_info->username = strdup(username);
    user_info->message = strdup(line);

    int rc = send_message(socket_fd, user_info);
    if (rc == -1) {
      perror("Failed to send message to server");
      exit(EXIT_FAILURE);
    }
  }

  printf("Socket is closed.\n");

  close(socket_fd);

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
    user_info_t* user_info = receive_message(socket_fd);
    
    if (user_info == NULL) {
      break;
    }
    printf("%s: %s\n", user_info->username, user_info->message);

    // Free everything received
    free(user_info->username);
    free(user_info->message);
    free(user_info);
  }
  
  return NULL;
}

int main(int argc, char** argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <username> <server name> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  username = argv[1];

  // Read command line arguments
  char* server_name = argv[2];
  unsigned short port = atoi(argv[3]);

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

  printf("Client should be closed now.\n");

  return 0;
}
