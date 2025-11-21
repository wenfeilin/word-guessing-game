#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>

#include "message.h"
#include "socket.h"

typedef struct user_node {
  int socket_fd;
  struct user_node* next;
} user_node_t;

/**
 * This struct is the root of the data structure that will hold users and hashed passwords.
 * It carries pointer to the first user, and the numeber of users in the set.
 * Our password ser is a linked list structure.
 */
typedef struct user_set {
  user_node_t* firstUser;
  int numUsers;
} user_set_t;

typedef struct user_thread {
  int client_socket_fd;
  user_set_t* chatUsers;
} user_thread_t;

void* worker(void* args) {
  user_thread_t* user_args = (user_thread_t *) args;

  // Read a message from the client
  char* message = receive_message(user_args->client_socket_fd);

  while (strcmp(message, "quit") != 0) {
    if (message == NULL) {
      perror("Failed to read message from client");
      exit(EXIT_FAILURE);
    }

    // Capitalize message
    for (char* s = message; *s != '\0'; s++) {
      *s = (char) toupper((int)*s);
    }

    user_node_t* current = user_args->chatUsers->firstUser;
    while (current != NULL) {
      int rc = send_message(current->socket_fd, message);
      if (rc == -1) {
        perror("Failed to send message to client");
        exit(EXIT_FAILURE);
      }
      current = current->next;
    }
    
    message = receive_message(user_args->client_socket_fd);
  }

  // Print the message
  printf("Client sent: %s\n", message);

  // Free the message string
  free(message);

  // Close sockets
  close(user_args->client_socket_fd);
  free(user_args);

  return NULL;
} 

int main() {
  // Open a server socket
  unsigned short port = 0;
  int server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1) {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // Start listening for connections, with a maximum of one queued connection
  if (listen(server_socket_fd, 1)) {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %u\n", port);

  user_set_t* users = (user_set_t*) malloc(sizeof(user_set_t));
  users->firstUser = NULL;
  users->numUsers = 0;

  while (true) {
    // Wait for a client to connect
    int* client_fd_copy = (int *) malloc(sizeof(int));
    user_thread_t* user_args = (user_thread_t*) malloc(sizeof(user_thread_t));

    int client_socket_fd = server_socket_accept(server_socket_fd);
    *client_fd_copy = client_socket_fd;

    user_node_t* newUser = malloc(sizeof(user_node_t));
    newUser->socket_fd = *client_fd_copy;

    user_args->chatUsers = users;
    user_args->client_socket_fd = *client_fd_copy;

    if (users->firstUser == NULL) {
      users->firstUser = newUser;
    } else {
      user_node_t* current = users->firstUser;
      while (current->next != NULL) current = current->next;
      current->next = newUser;
    }
    users->numUsers++;

    if (client_socket_fd == -1) {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    printf("Client connected!\n");

    pthread_t thread;
    pthread_create(&thread, NULL, worker, user_args);

    // Send a message to the client

    // int rc = send_message(client_socket_fd, "Hello client!");
    // if (rc == -1) {
    //   perror("Failed to send message to client");
    //   exit(EXIT_FAILURE);
    // }
  }

  
  free (users);
  close(server_socket_fd);

  return 0;
}
