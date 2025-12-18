#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "message.h"
#include "socket.h"

/*******************
 * Global variables
 *******************/
// Keep the username in a global so we can access it
const char *username;

/**
 * Read in user input and send that message to the server.
 */
void* send_to_server (void* args) {
  int* socket = (int *) args;
  int socket_fd = *socket;

  char* line = NULL;
  size_t size = 0;
  
  // Get user input.
  while (getline(&line, &size, stdin)) {
    line[strlen(line) - 1] = '\0';

    // if (strcmp(line, "quit") == 0) { // Prompt to quit program
    //   printf("Broke in send_to_server\n");
    //   break;
    // }

    user_info_t* user_info = malloc(sizeof(user_info_t));
    user_info->username = strdup(username);
    user_info->message = strdup(line);

    // Send message to server.
    int rc = send_message(socket_fd, user_info);
    if (rc == -1) {
      perror("Failed to send message to server");
      close(socket_fd); // Close client's side of the socket connecting to server
      exit(EXIT_FAILURE);
    }

    free(user_info->username);
    free(user_info->message);
    free(user_info);

    if (strcmp(line, "quit") == 0) { // Prompt to quit program
      // printf("Broke in send_to_server\n");
      break;
    }
  }

  close(socket_fd);
  printf("Socket is closed.\n");
  free(line);
  return NULL;
}

/**
 * Read message sent from server and display it in the chat.
 */
void* read_from_server (void* args) {
  int* socket = (int *) args;
  int socket_fd = *socket;

  char* line = NULL;
  size_t size = 0;
  
  while (true) {
    // Receive a message from the server
    user_info_t* user_info = receive_message(socket_fd);
    
    // If there is an error in receiving a message, ...
    if (user_info == NULL) {
      break;
    }

    // Display message from server.
    printf("%s: %s\n", user_info->username, user_info->message);

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
  
  // Begin sending and reading messages to/from the server.
  pthread_t send_message_thread;
  pthread_t read_message_thread;
  pthread_create(&send_message_thread, NULL, send_to_server, &socket_fd);
  pthread_create(&read_message_thread, NULL, read_from_server, &socket_fd);

  // Wait for both threads to finish execution before closing the client.
  pthread_join(send_message_thread, NULL);
  pthread_join(read_message_thread, NULL);

  printf("Client should be closed now.\n");
  return 0;
}