#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>

#include "message.h"
#include "socket.h"
#include "user.h"

// for linked list
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

typedef struct user_thread { // "server info"
  int client_socket_fd; // the connecting user (changes every time we get a new connection)
  user_set_t* chatUsers; // list of users
} user_thread_t;

// Global variables
user_thread_t* server_info = NULL;
pthread_mutex_t users_lock; // Lock to protect modification of list of users.


// Helper Functions
void remove_user(int user_to_delete_fd) {
  pthread_mutex_lock(&users_lock);

  if (server_info->chatUsers->firstUser == NULL) {
    printf("not good NULL\n");
  }

  user_node_t* curr = server_info->chatUsers->firstUser;
  user_node_t* prev = NULL;

  while(curr != NULL) {
    if (curr->socket_fd == user_to_delete_fd) {
      user_node_t* temp = curr->next;
      prev->next = curr->next;
      free(curr);
      curr = temp;
    } else {
      prev = curr;
      curr = curr->next;
    }
  }
  server_info->chatUsers->numUsers--;

  pthread_mutex_unlock(&users_lock);
}

void* worker(void* args) {
  user_thread_t* server_info = (user_thread_t *) args;

  // Read a message from the client
  user_info_t* user_info = receive_message(server_info->client_socket_fd);

  while (true) {
    // Remove the user if there's some error when trying to receive a message from it.
    if (user_info == NULL) {
      pthread_mutex_lock(&users_lock);
      // Remove it from the list of users.
      printf("Before remove: \n");

      user_node_t* curr1 = server_info->chatUsers->firstUser;
      while (curr1 != NULL) {
        printf("Socket: %d\n", curr1->socket_fd);
        curr1 = curr1->next;
      }

      remove_user(server_info->client_socket_fd);

      printf("After remove: \n");

      user_node_t* curr2 = server_info->chatUsers->firstUser;
      while (curr2 != NULL) {
        printf("Socket: %d\n", curr2->socket_fd);
        curr2 = curr2->next;
      }

      pthread_mutex_unlock(&users_lock);


      // close(server_info->client_socket_fd)
      // Close sockets
      close(server_info->client_socket_fd);
      break;
    } else {
      // Send message to all clients.
      pthread_mutex_lock(&users_lock);
      user_node_t* current = server_info->chatUsers->firstUser;
      while (current != NULL) {
        // printf("%d\n", current->socket_fd);
        printf("num users: %d\n", server_info->chatUsers->numUsers);
        int rc = send_message(current->socket_fd, user_info);
        if (rc == -1) {
          perror("Failed to send message to client");
          exit(EXIT_FAILURE);

          // remove_user(server_info->client_socket_fd); // REMOVE IF IT DOESNT WORK
        }
        current = current->next;
      }
      pthread_mutex_unlock(&users_lock);
      
      user_info = receive_message(server_info->client_socket_fd);
      
      // Free the message string
      // free(user_info->message);
      // free(user_info);
    }
  }

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

  pthread_mutex_init(&users_lock, NULL);

  // Wait for a client to connect
  server_info = (user_thread_t*) malloc(sizeof(user_thread_t));

  while (true) {
    // Accept connection from user
    int client_socket_fd = server_socket_accept(server_socket_fd);
    // int client_fd_copy = client_socket_fd;

    // Add user to list of users.
    user_node_t* newUser = malloc(sizeof(user_node_t));
    newUser->socket_fd = client_socket_fd;
    newUser->next = NULL;

    pthread_mutex_lock(&users_lock);
    server_info->chatUsers = users;
    server_info->client_socket_fd = client_socket_fd;

    if (users->firstUser == NULL) {
      users->firstUser = newUser;
      printf("Should go in here\n");
    } else {
      user_node_t* current = users->firstUser;

      while (current->next != NULL) current = current->next;
      current->next = newUser;
    }
    users->numUsers++;

    printf("After new connection added: \n");

    user_node_t* curr2 = server_info->chatUsers->firstUser;
    while (curr2 != NULL) {
      printf("Socket: %d\n", curr2->socket_fd);
      curr2 = curr2->next;
    }

    printf("End printing\n");

    pthread_mutex_unlock(&users_lock);

    if (client_socket_fd == -1) {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    printf("Client connected!\n");

    pthread_t thread;
    pthread_create(&thread, NULL, worker, server_info);
  }

  free(server_info);
  close(server_socket_fd);

  return 0;
}
