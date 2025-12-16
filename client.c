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
pthread_mutex_t can_send_msg_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t can_send_msg = PTHREAD_COND_INITIALIZER;
bool has_sent_msg = false;

/**
 * Read in user input and send that message to the server.
 */
void* send_to_server (void* args) {
  int* socket = (int *) args;
  int socket_fd = *socket;

  char* line = NULL;
  size_t size = 0;
  
  // Make this player unable to send messages to the server (it won't go through) as long as the 
  // server hasn't sent the player a message prompting them for input (a Y/N answer or question).
  pthread_mutex_lock(&can_send_msg_lock);
  while (has_sent_msg == false) {
    pthread_cond_wait(&can_send_msg, &can_send_msg_lock);
  }

  has_sent_msg = false; // Reset ability to send messages to server.
  pthread_mutex_unlock(&can_send_msg_lock);

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

    if (strcmp(line, "quit") == 0) { // Prompt to quit program
      // printf("Broke in send_to_server\n");
      break;
    }
  }

  close(socket_fd);
  printf("Socket is closed.\n");

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
      // printf("Broke in read_from_server\n");
      break;
    }

    // printf("should be true: %d\n", strcmp(user_info->username, "Server") == 0 && 
    //     (strcmp(user_info->message, "You are the host. Pick your secret word.") == 0));

    // Only be allowed to send message if picking the secret word as the host or when it's 
    // user's turn to ask question.
    if (strcmp(user_info->username, "Server") == 0 && 
        (strcmp(user_info->message, "You are the host. Pick your secret word.") == 0 ||
        strcmp(user_info->message, "It is your turn to ask the host a Yes/No question about the secret word.") == 0 ||
        strcmp(user_info->message, "It is time to make your guess for the secret word.") == 0)) {

      // Signal that this player is allowed to send messages to the server.
      pthread_mutex_lock(&can_send_msg_lock);
      has_sent_msg = true;
      pthread_cond_broadcast(&can_send_msg); // wake up all threads
      pthread_mutex_unlock(&can_send_msg_lock);
    }

    // Display message from server.
    printf("%s: %s\n", user_info->username, user_info->message);

    // Free everything the message info.
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

  // Maybe free stuff here and close user's end of the socket connecting to server?

  return 0;
}
