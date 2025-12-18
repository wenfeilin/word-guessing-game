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
  bool is_game_initialized;
  user_node_t* curr_host; 
  user_node_t* curr_asker;
  char* secret_word;
  int curr_question; // asked (for all rounds of guessing)
  int max_questions; // asked (for all rounds of guessing)
  bool is_receiving_secret_word;
  bool is_guessing;
  bool guessed_secret_word;
  bool asker_updated;
  bool host_updated;
  user_node_t* leading_player;
  char* leading_username;
  bool end_game;
} server_info_t;


/*******************
 * Global variables
 *******************/
server_info_t* server_info_global = NULL;
pthread_mutex_t server_info_global_lock; // A lock that should be used to protect the modification of the server info struct.


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

  if (user_to_delete_fd == server_info_global->curr_asker->socket_fd) {
  // Proceed to the next asker for question asking.
      if (server_info_global->curr_asker->next != NULL) {
        server_info_global->curr_asker = server_info_global->curr_asker->next;
      } else {
        server_info_global->curr_asker = server_info_global->chat_users->first_user;
      }

      // If everyone has asked their question (the asker loops back around to the host), 
      // then begin the next round of asking, starting with the first asker of the previous round.
      if (server_info_global->curr_asker->socket_fd == server_info_global->curr_host->socket_fd) {
        if (server_info_global->curr_asker->next != NULL) {
          server_info_global->curr_asker = server_info_global->curr_asker->next;
        } else {
          server_info_global->curr_asker = server_info_global->chat_users->first_user;
        }
      }
  }


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

    // printf("Users left (after removal): %d\n", server_info_global->chat_users->numUsers);
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

  pthread_mutex_unlock(&server_info_global_lock);
}

/**
 * Add a new player to the game.
 * 
 * \param users A linked list of the players of the game
 * \param new_user_socket_fd The socket file descriptor of the new player
 */
void add_player_to_list(user_list_t* users, int new_user_socket_fd) {
  // Create a node for the new user.
  user_node_t* newUser = malloc(sizeof(user_node_t));
  newUser->score = 0;
  newUser->socket_fd = new_user_socket_fd;
  newUser->next = NULL;

  // Add user to list of users.
  pthread_mutex_lock(&server_info_global_lock);
  if (users->first_user == NULL) { // First connecting user
    users->first_user = newUser;
    server_info_global->leading_player = newUser;
    server_info_global->leading_username = malloc(sizeof(char));
  } else { // Subsequent connecting users
    user_node_t* current = users->first_user;

    while (current->next != NULL) {
      current = current->next;
    }

    // Add new user to the end of the list of users.
    current->next = newUser;
  }

  // Increment user count.
  users->numUsers++;
  pthread_mutex_unlock(&server_info_global_lock);
}


/**
 * Change the asker to the next player in the list of users and indicate that the asker has been 
 * updated.
 */
void update_asker() {
  // Proceed to the next asker for question asking.
  if (server_info_global->curr_asker->next != NULL) {
    server_info_global->curr_asker = server_info_global->curr_asker->next;
  } else {
    server_info_global->curr_asker = server_info_global->chat_users->first_user;
  }

  // If everyone has asked their question (the asker loops back around to the host), 
  // then begin the next round of asking, starting with the first asker of the previous 
  // round.
  if (server_info_global->curr_asker->socket_fd == server_info_global->curr_host->socket_fd) {
    if (server_info_global->curr_asker->next != NULL) {
      server_info_global->curr_asker = server_info_global->curr_asker->next;
    } else {
      server_info_global->curr_asker = server_info_global->chat_users->first_user;
    }
  }

  // Indicate that the asker has been changed at this point.
  server_info_global->asker_updated = true;
}

/**
 * Prepare for the next round by changing the host to the next player in the list of users, 
 * signaling that the server should receive the secret word next, changing the next asker, 
 * and indicating that the asker and host have been updated. 
 */
void set_up_for_next_round() {
  // Update the host for the next round.
  server_info_global->curr_host = server_info_global->curr_host->next;
  server_info_global->host_updated = true; // Indicate that there is a new host.

  // Signal that a secret word has to be selected (before the round begins).
  server_info_global->is_receiving_secret_word = true;
  server_info_global->curr_question = 0;
  server_info_global->guessed_secret_word = false;

  // Proceed to the next asker for question asking.
  if (server_info_global->curr_asker->next != NULL) {
    server_info_global->curr_asker = server_info_global->curr_asker->next;
  } else {
    server_info_global->curr_asker = server_info_global->chat_users->first_user;
  }

  // If everyone has asked their question (the asker loops back around to the host), 
  // then begin the next round of asking, starting with the first asker of the previous round.
  if (server_info_global->curr_asker->socket_fd == server_info_global->curr_host->socket_fd) {
    if (server_info_global->curr_asker->next != NULL) {
      server_info_global->curr_asker = server_info_global->curr_asker->next;
    } else {
      server_info_global->curr_asker = server_info_global->chat_users->first_user;
    }
  }

  server_info_global->asker_updated = true; // Indicate that there is a new asker.
}

/**
 * End the game by announcing the game's winner, sending each individual player their score, and 
 * disconnecting everyone from the server at the end.
 * 
 * \param server_info A local copy of the global server info struct containing the game state/info
 */
void end_game(server_info_t* server_info) {
  // Print player's own score locally and the winner's score & username globally.
  char* start_of_msg = "The game has ended.\n";
  char* game_winner_msg = " is the winner of the game with ";
  char* rest_of_msg = " points!\n";

  int message_len = strlen(start_of_msg) + strlen(server_info_global->leading_username) + strlen(game_winner_msg) + sizeof(int) * 2 + strlen(rest_of_msg) + 1; // 1 = null char
  char *buf = malloc(sizeof(char) * message_len);
  snprintf(buf, message_len, "The game has ended.\n%s is the winner of the game with %d points!", server_info_global->leading_username, server_info_global->leading_player->score);

  user_info_t * winner_of_game = malloc(sizeof(user_info_t));
  winner_of_game->username = "Server";
  winner_of_game->message = buf;

  user_node_t* curr = server_info->chat_users->first_user;

  while (curr != NULL) {
    // Send message announcing game winner.
    int rc = send_message(curr->socket_fd, winner_of_game);

    if (rc == -1) {
      // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
      perror("Failed to send message to client");
      exit(EXIT_FAILURE);
    }

    char* local_score_msg = "Your score: ";

    int msg_len = strlen(local_score_msg) + sizeof(int) * 2 + 1; // 2 = number of digits for the score (random for now), 1 = null char
    char *buf2 = malloc(sizeof(char) * msg_len);
    snprintf(buf2, msg_len, "Your score: %d", curr->score);

    user_info_t * own_score = malloc(sizeof(user_info_t));
    own_score->username = "Server";
    own_score->message = buf2;

    // Send message showing own score.
    rc = send_message(curr->socket_fd, own_score);
    free(buf2);
    free(own_score);

    if (rc == -1) {
      // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
      perror("Failed to send message to client");
      exit(EXIT_FAILURE);
    }

    // Kick everyone out.
    close(curr->socket_fd);

    curr = curr->next;
  }

  user_node_t* curr2 = server_info->chat_users->first_user;

  while (curr2 != NULL) {
    user_node_t *temp = curr2;
    remove_user(curr2->socket_fd);
    curr2 = temp->next;
  }

  free(buf);
  free(winner_of_game);



  // Free-ing memory.
}

/**
 * Validate the guesses by adding a point to the player who successfully guesses the secret word, 
 * announcing the round's winner to everyone, and updating the new leading player of the game. 
 * Everyone who tried to but failed to guess the secret word correctly is also told to try again.
 * 
 * \param server_info A local copy of the global server info struct containing the game state/info
 * \param user_info A structure containing the guess and username of the player who made the guess
 * \param user_socket_fd The socket file descriptor of the player making the guess
 */
void validate_guesses(server_info_t* server_info, user_info_t* user_info, int user_socket_fd) {
  if (strcasecmp(user_info->message, server_info->secret_word) == 0) {
    pthread_mutex_lock(&server_info_global_lock);
    server_info_global->guessed_secret_word = true;
    server_info_global->is_guessing = false;

    user_node_t* current = server_info_global->chat_users->first_user;

    char* rest_of_message = " is the winner of this round!";
    char *result = malloc(strlen(user_info->username) + strlen(rest_of_message) + 1);
    strcpy(result, user_info->username);
    strcat(result, rest_of_message);

    user_info_t* server_round_winner_msg = malloc(sizeof(user_info_t));
    server_round_winner_msg->username = strdup("Server");
    server_round_winner_msg->message = strdup(result);
    free(result);

    // Calculate new scores.
    while (current != NULL) {
      // Announce to everyone the winner of this round (for the secret word).
      int rc = send_message(current->socket_fd, server_round_winner_msg);

      if (rc == -1) {
        // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
        perror("Failed to send message to client");
        exit(EXIT_FAILURE);
      }

      if (current->socket_fd == user_socket_fd) {
        current->score++;
      }

      if (current->score > server_info_global->leading_player->score) {
        server_info_global->leading_player = current;
        free(server_info_global->leading_username);
        server_info_global->leading_username = strdup(user_info->username);
      }

      current = current->next;
    }

    if (server_info_global->curr_host->next == NULL) {
      server_info_global->end_game = true;
    }

    free(server_round_winner_msg->username);
    free(server_round_winner_msg->message);
    free(server_round_winner_msg);

    pthread_mutex_unlock(&server_info_global_lock);
  } else {
    user_info_t* server_try_again_msg = malloc(sizeof(user_info_t));
    server_try_again_msg->username = strdup("Server");
    server_try_again_msg->message = strdup("Wrong guess. Try again!");

    int rc = send_message(user_socket_fd, server_try_again_msg);

    if (rc == -1) {
      // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
      perror("Failed to send message to client");
      exit(EXIT_FAILURE);
    }

    free(server_try_again_msg->username);
    free(server_try_again_msg->message);
    free(server_try_again_msg);
  }
}

/**
 * Thread Worker Functions (Core Functions)
 */

void* start_game(void* args) {
  server_info_t* server_info = (server_info_t *) args;

  // Pick the first host to start the game.
  pthread_mutex_lock(&server_info_global_lock);
  server_info_global->curr_host = server_info_global->chat_users->first_user;

  // printf("Curr host: %d\n", server_info_global->chat_users->first_user->socket_fd);
  pthread_mutex_unlock(&server_info_global_lock);

  user_info_t* server_pick_secret_msg = malloc(sizeof(user_info_t));
  server_pick_secret_msg->username = strdup("Server");
  server_pick_secret_msg->message = strdup("You are the host. Pick your secret word.");

  // Send a message to the first host to pick a secret word.
  pthread_mutex_lock(&server_info_global_lock);
  int rc = send_message(server_info_global->curr_host->socket_fd, server_pick_secret_msg);
  pthread_mutex_unlock(&server_info_global_lock);

  if (rc == -1) {
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }

  free(server_pick_secret_msg->username);
  free(server_pick_secret_msg->message);
  free(server_pick_secret_msg);

  // Receive the secret word from the host.
  pthread_mutex_lock(&server_info_global_lock);
  user_info_t* user_info = receive_message(server_info_global->curr_host->socket_fd);
  pthread_mutex_unlock(&server_info_global_lock);

  // Send message to all players, except the host, signaling the start of the game.
  pthread_mutex_lock(&server_info_global_lock);
  user_node_t* current = server_info_global->chat_users->first_user;
  pthread_mutex_unlock(&server_info_global_lock);

  user_info_t* server_start_game_msg = malloc(sizeof(user_info_t));
  server_start_game_msg->username = strdup("Server");
  server_start_game_msg->message = strdup("The game has started. Wait for your turn to ask a question about the secret word.");

  while (current != NULL) {
    pthread_mutex_lock(&server_info_global_lock);
    if (current->socket_fd != server_info_global->curr_host->socket_fd) {

      int rc = send_message(current->socket_fd, server_start_game_msg);

      if (rc == -1) {
        // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
        perror("Failed to send message to client");
        exit(EXIT_FAILURE);
      }
    }

    current = current->next;
    
    pthread_mutex_unlock(&server_info_global_lock);
  }

  free(server_start_game_msg->username);
  free(server_start_game_msg->message);
  free(server_start_game_msg);
  
  // Save the secret word.
  pthread_mutex_lock(&server_info_global_lock);
  server_info_global->secret_word = strdup(user_info->message);

  printf("The secret word is %s\n", server_info_global->secret_word);
  pthread_mutex_unlock(&server_info_global_lock);

  free(user_info->username);
  free(user_info->message);
  free(user_info);


  // Set the first asker (as the next player after the host in the linked list).
  pthread_mutex_lock(&server_info_global_lock);
  server_info_global->curr_asker = server_info_global->curr_host->next;
  pthread_mutex_unlock(&server_info_global_lock);


  // Tell current asker to send a question.
  user_info_t* server_start_asking_msg = malloc(sizeof(user_info_t));
  server_start_asking_msg->username = strdup("Server");
  server_start_asking_msg->message = strdup("It is your turn to ask the host a Yes/No question about the secret word.");

  pthread_mutex_lock(&server_info_global_lock);
  rc = send_message(server_info_global->curr_asker->socket_fd, server_start_asking_msg);
  pthread_mutex_unlock(&server_info_global_lock);


  if (rc == -1) {
    // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }

  free(server_start_asking_msg->username);
  free(server_start_asking_msg->message);
  free(server_start_asking_msg);
  

  // Loop through list of players and create a thread for each so that they can start communicating w/ e/o.
  pthread_mutex_lock(&server_info_global_lock);
  user_node_t * curr = server_info_global->chat_users->first_user;
  
  while (curr != NULL) {
    pthread_t forward_msg_thread;
    pthread_create(&forward_msg_thread, NULL, forward_msg, &curr->socket_fd);

    curr = curr->next;
  }
  pthread_mutex_unlock(&server_info_global_lock);
  
  return NULL;
}

/**
 * Receives and sends user's message to all other users.
 */
void* forward_msg(void* args) {
  int* user_socket = (int *) args;
  int user_socket_fd = *user_socket;

  // Create local copy of server_info to ONLY read data from the struct w/o needing to lock.
  pthread_mutex_lock(&server_info_global_lock);
  server_info_t* server_info = server_info_global;
  pthread_mutex_unlock(&server_info_global_lock);

  while (true) {
    // Read a message from the player.
    user_info_t* user_info = receive_message(user_socket_fd);
    printf("message received from player: %s\n", user_info->message);

    // Validate the guesses received against the secret word (in guessing round).
    if (server_info->is_guessing) {
      validate_guesses(server_info, user_info, user_socket_fd);
    }

    // Save the new secret word if the game is currently in the process of starting a new round w/ 
    // a new host.
    if (server_info->is_receiving_secret_word) {
      pthread_mutex_lock(&server_info_global_lock);
      free(server_info_global->secret_word);
      server_info_global->secret_word = strdup(user_info->message);
      pthread_mutex_unlock(&server_info_global_lock);
    }


    // Remove the user if there's some error when trying to receive a message from it or 
    // the user is quitting the game.
    if (user_info == NULL || strcmp(user_info->message, "quit") == 0) {
      remove_user(user_socket_fd);
      // Close server's end of the socket.
      close(user_socket_fd);
      break;
    } else {

      user_node_t* current = server_info->chat_users->first_user;
    

      pthread_mutex_lock(&server_info_global_lock);
      // Only don't forward a user's message to all users if the message is the secret word.
      if (!server_info_global->is_receiving_secret_word && !server_info_global->is_guessing && ((user_socket_fd == server_info_global->curr_asker->socket_fd) || (user_socket_fd == server_info_global->curr_host->socket_fd))) {

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
      } 
      

      if (!server_info_global->is_receiving_secret_word && !server_info_global->is_guessing && !server_info_global->end_game) {
        if ((user_socket_fd != server_info_global->curr_asker->socket_fd) && (user_socket_fd != server_info_global->curr_host->socket_fd)) {
          user_info_t* not_turn_msg = malloc(sizeof(user_info_t));
          not_turn_msg->username = strdup("Server");
          not_turn_msg->message = strdup("It is not your turn yet. Please wait.");

          int rc = send_message(user_socket_fd, not_turn_msg);

          if (rc == -1) {
            perror("Failed to send message to client");
            exit(EXIT_FAILURE);
          }

          free(not_turn_msg->username);
          free(not_turn_msg->message);
          free(not_turn_msg);
        }
      }

      if (server_info_global->is_receiving_secret_word) {
        server_info_global->is_receiving_secret_word = false;
      }

      pthread_mutex_unlock(&server_info_global_lock);

      // The current host should always be sending a Y/N answer.
      if (user_socket_fd == server_info->curr_host->socket_fd &&
          (strcmp(user_info->message, "y") == 0 || strcmp(user_info->message, "n") == 0)) {
        // Change current asker
      
        pthread_mutex_lock(&server_info_global_lock);
        // Update the number of questions the host has answered.
        server_info_global->curr_question++;

        // Update the current asker.
        update_asker();
        pthread_mutex_unlock(&server_info_global_lock);

        pthread_mutex_lock(&server_info_global_lock);
        if (server_info_global->curr_question == server_info_global->max_questions /*&& 
            server_info_global->curr_host->next != NULL*/) {
          server_info_global->is_guessing = true;
          
          user_info_t* server_start_guessing_msg = malloc(sizeof(user_info_t));
          server_start_guessing_msg->username = strdup("Server");
          server_start_guessing_msg->message = strdup("It is time to make your guess for the secret word.");

          user_node_t* current = server_info->chat_users->first_user;

          // All non-host players can begin making their guess.
          while (current != NULL) {
            if (current != server_info->curr_host) {
              int rc = send_message(current->socket_fd, server_start_guessing_msg);

              if (rc == -1) {
                // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
                perror("Failed to send message to client");
                exit(EXIT_FAILURE);
              }
            }

            current = current->next;
          }

          free(server_start_guessing_msg->username);
          free(server_start_guessing_msg->message);
          free(server_start_guessing_msg);
        }
        pthread_mutex_unlock(&server_info_global_lock);
      }

      free(user_info->username);
      free(user_info->message);
      free(user_info);

      pthread_mutex_lock(&server_info_global_lock);
      if (server_info_global->guessed_secret_word && 
          server_info_global->curr_host->next != NULL) {
        // pthread_mutex_lock(&server_info_global_lock);
        set_up_for_next_round();
      } else if (server_info_global->guessed_secret_word && server_info_global->curr_host->next == NULL) { // done with game
        end_game(server_info);
      }
      
      // Every time a player becomes the current asker, tell the player to send a question.
      if (server_info_global->asker_updated && (server_info->curr_question < server_info->max_questions) && !server_info_global->is_receiving_secret_word) {
        user_info_t* server_start_asking_msg = malloc(sizeof(user_info_t));
        server_start_asking_msg->username = strdup("Server");
        server_start_asking_msg->message = strdup("It is your turn to ask the host a Yes/No question about the secret word.");

        int rc = send_message(server_info_global->curr_asker->socket_fd, server_start_asking_msg);

        if (rc == -1) {
          // remove_user(user_socket_fd); // DO WE WANT TO REMOVE USER IF SENDING FAILS OR DO STH ELSE?
          perror("Failed to send message to client");
          exit(EXIT_FAILURE);
        }

        free(server_start_asking_msg->message);
        free(server_start_asking_msg->username);
        free(server_start_asking_msg);

        server_info_global->asker_updated = false;
      }

      // Every time a player becomes the new host, tell the player to set a secret word.
      if (server_info_global->host_updated) {
        user_info_t* server_pick_secret_msg = malloc(sizeof(user_info_t));
        server_pick_secret_msg->username = strdup("Server");
        server_pick_secret_msg->message = strdup("You are the host. Pick your secret word.");

        int rc = send_message(server_info_global->curr_host->socket_fd, server_pick_secret_msg);

        if (rc == -1) {
          perror("Failed to send message to client");
          exit(EXIT_FAILURE);
        }

        free(server_pick_secret_msg->username);
        free(server_pick_secret_msg->message);
        free(server_pick_secret_msg);

        server_info_global->host_updated = false;
      }
      pthread_mutex_unlock(&server_info_global_lock);

      // Update the local copy to be equal to current state of global game state.
      server_info = server_info_global;
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
  server_info_global->is_game_initialized = false;
  server_info_global->curr_question = 0;
  server_info_global->max_questions = 1;
  server_info_global->is_receiving_secret_word = false;
  server_info_global->is_guessing = false;
  server_info_global->guessed_secret_word = false;
  server_info_global->asker_updated = false;
  server_info_global->host_updated = false;
  server_info_global->end_game = false;


  pthread_mutex_init(&server_info_global_lock, NULL);

  // Continuously wait for a client to connect.
  while (true) {
    pthread_mutex_lock(&server_info_global_lock);
    // Check if there are enough players connected to start the game.
    if (server_info_global->chat_users->numUsers >= 2 && !server_info_global->is_game_initialized) { 
      server_info_global->is_game_initialized = true;
      // Create thread to start game.
      pthread_t thread;
      pthread_create(&thread, NULL, start_game, server_info_global);
    }
    pthread_mutex_unlock(&server_info_global_lock);

    // Accept connection from user.
    int client_socket_fd = server_socket_accept(server_socket_fd); 
    pthread_mutex_lock(&server_info_global_lock);
    server_info_global->connecting_user_socket_fd = client_socket_fd;

    user_info_t* welcome_msg = malloc(sizeof(user_info_t));
    welcome_msg->username = strdup("Server");
    welcome_msg->message = strdup("Welcome to the Guessing Secret Word game!\n Each player will take turn to be the host and pick a secret word.\n Other players will take turn to ask yes/no questions to guess the secret word.\n Whoever makes the most correct guesses will be the winner!\n");

    // Send a message to the first host to pick a secret word.
    int rc = send_message(client_socket_fd, welcome_msg);

    if (rc == -1) {
      perror("Failed to send message to client");
      exit(EXIT_FAILURE);
    }

    free(welcome_msg->username);
    free(welcome_msg->message);
    free(welcome_msg);

    pthread_mutex_unlock(&server_info_global_lock);

    // Create node for new user.
    user_node_t* newUser = malloc(sizeof(user_node_t));
    newUser->score = 0;
    newUser->socket_fd = client_socket_fd;
    newUser->next = NULL;

    // Add new player to list of players.
    add_player_to_list(users, client_socket_fd);

    // Connection was unsuccessful.
    if (client_socket_fd == -1) {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    printf("Client connected!\n");
  }

  // Traversing through the users linked list to free each node 
  user_node_t* current = server_info_global->chat_users->first_user;
  while (current != NULL) {
    user_node_t* temp = current->next;
    free(current);
    current = temp;
  }

  free(server_info_global->chat_users); // Freeing the linked list
  free(server_info_global->secret_word); // Freeing secret word
  free(server_info_global->leading_username); // Freeing leading user name
  free(server_info_global);
  close(server_socket_fd);

  return 0;
}