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

/*************************
 * Linked List Structures
 *************************/
// A node where its value is the socket file descriptor
typedef struct user_node {
  int socket_fd;
  int score;
  struct user_node* next;
} user_node_t;

// Linked list of users
typedef struct user_list {
  user_node_t* first_user;
  int numUsers;
} user_list_t;


/*************************
 * Server Info Structure
 *************************/
typedef struct server_info {
  int connecting_user_socket_fd; // The most recent connecting user (facilitates adding connections)
  user_list_t* chat_users; // List of currently connected users

  // Note: The order of asking q's = the order of linked list. Also order of being host = order of linked list.
  // Game-Related Info:
  user_node_t* curr_host; 
  user_node_t* curr_asker;
  char* secret_word;
} server_info_t;


/*******************
 * Global variables
 *******************/
server_info_t* server_info_global = NULL;
pthread_mutex_t server_info_global_lock; // A lock that should be used to protect the modification of the users list.


/*******************
 * Function Declarations
 *******************/
void* forward_msg(void* args);

/*******************
 * Helper Functions
 *******************/

/**
 * Removes a user from the list of users in server info.
 * 
 * \param user_to_delete_fd The file descriptor of the user to be deleted from the list
 */
void remove_user(int user_to_delete_fd) {
  pthread_mutex_lock(&server_info_global_lock);

  printf("Users left (before removal): %d\n", server_info_global->chat_users->numUsers);

  // Ensure the list of users isn't empty.
  if (server_info_global->chat_users->first_user == NULL) {
    perror("No user to delete. The list is empty.");
    exit(1);
  }

  // Code from GFG: https://www.geeksforgeeks.org/c/c-program-for-deleting-a-node-in-a-linked-list/
  // Store head node
  user_node_t *temp = server_info_global->chat_users->first_user, *prev;

  // Case 1: Deleting the first user.
  if (temp != NULL && temp->socket_fd == user_to_delete_fd) {
    server_info_global->chat_users->first_user = temp->next; // Changed head
    free(temp); // free old head
    server_info_global->chat_users->numUsers--;

    printf("Users left (after removal): %d\n", server_info_global->chat_users->numUsers);
    pthread_mutex_unlock(&server_info_global_lock);
    return;
  }

  // Case 2: Deleting user that is not the first.
  // Search for the user to be deleted; keep track of the previous node
  while (temp != NULL && temp->socket_fd != user_to_delete_fd) {
    prev = temp;
    temp = temp->next;
  }

  // If user to delete was not present in linked list
  if (temp == NULL) {
    pthread_mutex_unlock(&server_info_global_lock);
    return;
  }

  // Remove the user from the list
  prev->next = temp->next;

  free(temp); // Free memory

  // Decrement number of connected users.
  server_info_global->chat_users->numUsers--;

  printf("Users left (after removal): %d\n", server_info_global->chat_users->numUsers);

  pthread_mutex_unlock(&server_info_global_lock);
}

/**
 * Print every connected users' file descriptor.
 */
void print_users() {
  pthread_mutex_lock(&server_info_global_lock);
  // Loop through users list and print each one's file descriptor.
  user_node_t * curr = server_info_global->chat_users->first_user;
  int i = 1;
  
  while (curr != NULL) {
    printf("User %d's fd: %d\n", i, curr->socket_fd);
    i++;
    curr = curr->next;
  }
  pthread_mutex_unlock(&server_info_global_lock);
}

/**
 * Thread Worker Functions (Core Functions)
 */

void* start_game(void* args) {
  server_info_t* server_info = (server_info_t *) args;

  // Pick the host. Go in order of linked list of users.
  server_info->curr_host = server_info->chat_users->first_user;

  printf("Curr host: %d\n", server_info->chat_users->first_user->socket_fd);

  user_info_t* server_pick_secret_msg = malloc(sizeof(user_info_t));
  server_pick_secret_msg->username = strdup("Server");
  server_pick_secret_msg->message = strdup("You are the host. Pick your secret word.");

  int rc = send_message(server_info->curr_host->socket_fd, server_pick_secret_msg);

  if (rc == -1) {
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }

  // Receive the secret word from the host.
  user_info_t* user_info = receive_message(server_info->curr_host->socket_fd);

  if (user_info == NULL) {
    printf("user_info is NULL\n");
  }

  // Send message to all players, signaling start of game.
  pthread_mutex_lock(&server_info_global_lock);
  user_node_t* current = server_info_global->chat_users->first_user;
  pthread_mutex_unlock(&server_info_global_lock);

  user_info_t* server_start_game_msg = malloc(sizeof(user_info_t));
  server_start_game_msg->username = strdup("Server");
  server_start_game_msg->message = strdup("The game has started. Wait for your turn to ask a question about the secret word.");

  while (current != NULL) {
    if (current->socket_fd != server_info->curr_host->socket_fd) {

      int rc = send_message(current->socket_fd, server_start_game_msg);

      if (rc == -1) {
        // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
        perror("Failed to send message to client");
        exit(EXIT_FAILURE);
      }
    }

    current = current->next;
  }
  
  // Save the secret word.
  printf("the secret word?: %s\n", user_info->message);

  pthread_mutex_lock(&server_info_global_lock);
  server_info_global->secret_word = strdup(user_info->message);
  pthread_mutex_unlock(&server_info_global_lock);

  printf("The secret word is %s\n", server_info_global->secret_word);

  // Set first asker.
  pthread_mutex_lock(&server_info_global_lock);
  server_info_global->curr_asker = server_info_global->curr_host->next;
  pthread_mutex_unlock(&server_info_global_lock);


  // Tell current asker to send a question.
  user_info_t* server_start_asking_msg = malloc(sizeof(user_info_t));
  server_start_asking_msg->username = strdup("Server");
  server_start_asking_msg->message = strdup("It is your turn to ask the host a Yes/No question about the secret word.");

  rc = send_message(server_info_global->curr_asker->socket_fd, server_start_asking_msg);

  if (rc == -1) {
    // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }

  // Keep waiting until current asker sends a question.
  

  // Loop through users list and create a thread to start communication w/ other users for each.
  pthread_mutex_lock(&server_info_global_lock);
  user_node_t * curr = server_info_global->chat_users->first_user;
  
  while (curr != NULL) {
    pthread_t forward_msg_thread;
    pthread_create(&forward_msg_thread, NULL, forward_msg, &curr->socket_fd);
    curr = curr->next;
  }
  pthread_mutex_unlock(&server_info_global_lock);
  

  

  // Create threads for game logic.
  return NULL;
}

/**
 * Receives and sends user's message to all other users.
 */
void* forward_msg(void* args) {
  int* user_socket = (int *) args;
  int user_socket_fd = *user_socket;

  while (true) {
    // Read a message from the client
    user_info_t* user_info = receive_message(user_socket_fd);

    // Remove the user if there's some error when trying to receive a message from it.
    if (user_info == NULL || strcmp(user_info->message, "quit") == 0) {
      // printf("Before removal:\n");
      // print_users();
      remove_user(user_socket_fd);
      // print_users();
      // printf("After removal:\n");

      // Close server's end of the socket.
      close(user_socket_fd);
      break;
    } else {
      pthread_mutex_lock(&server_info_global_lock);
      user_node_t* current = server_info_global->chat_users->first_user;
      pthread_mutex_unlock(&server_info_global_lock);

      // Send message to all users.
      while (current != NULL) {
        int rc = send_message(current->socket_fd, user_info);

        if (rc == -1) {
          // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
          perror("Failed to send message to client");
          exit(EXIT_FAILURE);
        }

        current = current->next;
      }

      // Free the message string
      // free(server_info->message);
      // free(server_info);
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

  // Allocate space for server info.
  server_info_global = (server_info_t *) malloc(sizeof(server_info_t));

  // Initialize fields for server info and the lock.
  user_list_t* users = (user_list_t*) malloc(sizeof(user_list_t));
  users->first_user = NULL;
  users->numUsers = 0;
  server_info_global->chat_users = users;
  pthread_mutex_init(&server_info_global_lock, NULL);

  // Continuously wait for a client to connect.
  while (true) {
    pthread_mutex_lock(&server_info_global_lock);
    printf("Num users: %d\n", server_info_global->chat_users->numUsers);
    // Check if there are enough players connected to start the game.
    if (server_info_global->chat_users->numUsers >= 2) {
      // Create thread to start game.
      pthread_t thread;
      pthread_create(&thread, NULL, start_game, server_info_global);
    }
    pthread_mutex_unlock(&server_info_global_lock);

    // Accept connection from user.
    int client_socket_fd = server_socket_accept(server_socket_fd); // DO WE HAVE TO HAVE A LOCK FOR CHANGING FIELDS OF SERVER_INFO SINCE MULTIPLE THREADS WILL BE READING FROM & CHANGING IT...?
    pthread_mutex_lock(&server_info_global_lock);
    server_info_global->connecting_user_socket_fd = client_socket_fd;
    pthread_mutex_unlock(&server_info_global_lock);

    // Create node for new user.
    user_node_t* newUser = malloc(sizeof(user_node_t));
    newUser->socket_fd = client_socket_fd;
    newUser->next = NULL;

    // Add user to list of users.
    pthread_mutex_lock(&server_info_global_lock);

    if (users->first_user == NULL) { // First connecting user
      users->first_user = newUser;
    } else { // Subsequent connecting users
      user_node_t* current = users->first_user;

      while (current->next != NULL) {
        current = current->next;
      }

      // Add new user to the end of the list of users.
      current->next = newUser;
    }

    users->numUsers++;

    pthread_mutex_unlock(&server_info_global_lock);

    // Connection was unsuccessful.
    if (client_socket_fd == -1) {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    printf("Client connected!\n");
  }

  free(server_info_global);
  close(server_socket_fd);

  return 0;
}
