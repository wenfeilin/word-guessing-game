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

  // Note: The order of asking q's & being the host = the order of the nodes in the linked list.
  // Game-Related Info:
  bool is_game_initialized;
  user_node_t* curr_host; 
  user_node_t* curr_asker;
  char* secret_word;
  int curr_question; // answered by host for 1 round
  int max_questions; // answered by host for 1 round
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
server_info_t* server_info_global = NULL; // Global struct containing all game info
pthread_mutex_t server_info_global_lock; // A lock that should be used to protect the modification 
                                         // of the server info struct.


/************************
 * Function Declarations
 ************************/
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

  // To remove a node from a linked list requires two separate cases: deleting the head and 
  // deleting elsewhere.
  // Source:https://www.geeksforgeeks.org/c/c-program-for-deleting-a-node-in-a-linked-list/

  // Store head node.
  user_node_t *temp = server_info_global->chat_users->first_user, *prev;

  // Case 1: Deleting the first user.
  if (temp != NULL && temp->socket_fd == user_to_delete_fd) {
    server_info_global->chat_users->first_user = temp->next; // Changed head.
    free(temp); // free old head
    server_info_global->chat_users->numUsers--;
    pthread_mutex_unlock(&server_info_global_lock);
    return;
  }

  // Case 2: Deleting user that is not the first.
  // Search for the user to be deleted; keep track of the previous node.
  while (temp != NULL && temp->socket_fd != user_to_delete_fd) {
    prev = temp;
    temp = temp->next;
  }

  // If user to delete was not present in linked list.
  if (temp == NULL) {
    pthread_mutex_unlock(&server_info_global_lock);
    return;
  }

  // Remove the user from the list.
  prev->next = temp->next;

  free(temp); // Free memory.

  // Decrement number of connected users.
  server_info_global->chat_users->numUsers--;
  pthread_mutex_unlock(&server_info_global_lock);
}

/**
 * Send a message to a specified player.
 * 
 * \param sender The sender of the message
 * \param message The message to be sent
 * \param player_socket_fd The socket file descriptor of the player to send the message to
 */

void send_message_to_player(char* sender, char* message, int player_socket_fd) {
  // Create message from sender.
  user_info_t* msg_info = malloc(sizeof(user_info_t));
  msg_info->username = strdup(sender);
  msg_info->message = strdup(message);

  // Send the message to the host.
  int rc = send_message(player_socket_fd, msg_info);

  if (rc == -1) {
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }
}

/**
 * Send a message to all players.
 * 
 * \param server_info A local copy of the global server info struct containing the game state/info
 * \param message_info A structure containing the message and sender of the message
 */
void send_message_to_everyone(server_info_t* server_info, user_info_t* message_info) {
  user_node_t* current = server_info->chat_users->first_user;

  // Forward message to all users.
  while (current != NULL) {
    send_message_to_player(message_info->username, message_info->message, current->socket_fd);
    current = current->next;
  }
}

/**
 * Validate the guesses by adding a point to the player who successfully guesses the secret word, 
 * announcing the round's winner to everyone, and updating the new leading player of the game. 
 * Everyone who tried to but failed to guess the secret word correctly is also told to try again.
 * 
 * \param server_info A local copy of the global server info struct containing the game state/info
 * \param guess_info A structure containing the guess and username of the player who made the guess
 * \param player_socket_fd The socket file descriptor of the player making the guess
 */
void validate_guesses(server_info_t* server_info, user_info_t* guess_info, int player_socket_fd) {
  // Validate the guesses received against the secret word (when it is time to guess the 
  // secret word).
  if (strcasecmp(guess_info->message, server_info->secret_word) == 0) { // case-insensitive
    pthread_mutex_lock(&server_info_global_lock);
    server_info_global->guessed_secret_word = true; // The word has been guessed!
    server_info_global->is_guessing = false; // The guessing round ends.

    // Create the message announcing the winner of the round.
    char* rest_of_message = " is the winner of this round!";
    char *result = malloc(strlen(guess_info->username) + strlen(rest_of_message) + 1);
    strcpy(result, guess_info->username);
    strcat(result, rest_of_message);

    user_node_t* current = server_info_global->chat_users->first_user;

    // Calculate new scores.
    while (current != NULL) {
      // Announce to everyone the winner of this round (for the secret word).
      send_message_to_player("Server", result, current->socket_fd);

      // Update the score for the winner of the round.
      if (current->socket_fd == player_socket_fd) {
        current->score++;
      }

      // Update the current leading player of the game.
      if (current->score > server_info_global->leading_player->score) {
        server_info_global->leading_player = current;
        free(server_info_global->leading_username);
        server_info_global->leading_username = strdup(guess_info->username);
      }

      current = current->next;
    }

    free(result);

    // Indicate the end of the game once everyone has become the host once (and scores for 
    // the last round have been calculated).
    if (server_info_global->curr_host->next == NULL) {
      server_info_global->end_game = true;
    }

    pthread_mutex_unlock(&server_info_global_lock);
  } else {
    // Create message indicating the player wasn't able to guess the secret word.
    char* try_again_msg = "Wrong guess. Try again!";

    // Send the "Try again!" message to all players that are unsuccessful in guessing the word.
    send_message_to_player("Server", try_again_msg, player_socket_fd);
  }
}

/**
 * Change the asker to the next player in the list of users and indicate that the asker has been 
 * updated.
 */
void update_asker() {
  pthread_mutex_lock(&server_info_global_lock);
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
  pthread_mutex_unlock(&server_info_global_lock);
}

/**
 * Prepare for the next round by changing the host to the next player in the list of users, 
 * signaling that the server should receive the secret word next, changing the next asker, 
 * and indicating that the asker and host have been updated. 
 */
void set_up_for_next_round() {
  pthread_mutex_lock(&server_info_global_lock);
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
  pthread_mutex_unlock(&server_info_global_lock);
}

/**
 * End the game by announcing the game's winner, sending each individual player their score, and 
 * disconnecting everyone from the server at the end.
 * 
 * \param server_info A local copy of the global server info struct containing the game state/info
 */
void end_game(server_info_t* server_info) {
  // Print player's own score locally and the winner's score & username globally.

  // Create the global message that will announce the game's winner.
  char* start_of_msg = "The game has ended.\n";
  char* game_winner_msg = " is the winner of the game with ";
  char* rest_of_msg = " points!\n";

  // NOTE: We are assuming that the total points will be no more than 2 digits long.
  int message_len = strlen(start_of_msg) + strlen(server_info_global->leading_username) + 
                    strlen(game_winner_msg) + sizeof(int) * 2 + strlen(rest_of_msg) + 1;
  char *buf = malloc(sizeof(char) * message_len);
  snprintf(buf, message_len, 
            "The game has ended.\n%s is the winner of the game with %d points!", 
            server_info_global->leading_username, server_info_global->leading_player->score);

  user_info_t * winner_of_game = malloc(sizeof(user_info_t));
  winner_of_game->username = "Server";
  winner_of_game->message = buf;

  user_node_t* curr = server_info->chat_users->first_user;

  // Send the message announcing the game's winner to everyone.
  while (curr != NULL) {
    int rc = send_message(curr->socket_fd, winner_of_game);

    if (rc == -1) {
      perror("Failed to send message to client");
      exit(EXIT_FAILURE);
    }

    // Create the message showing the player's own score.
    // NOTE: We are assuming that the total points will be no more than 2 digits long.
    char* local_score_msg = "Your score: ";
    int msg_len = strlen(local_score_msg) + sizeof(int) * 2 + 1;
    char *own_score = malloc(sizeof(char) * msg_len);
    snprintf(own_score, msg_len, "Your score: %d", curr->score);

    // Send message showing own score (not announcing it to everyone).
    send_message_to_player("Server", own_score, curr->socket_fd);
    free(own_score);

    // Disconnect everyone out one-by-one since the game ended.
    close(curr->socket_fd);

    curr = curr->next;
  }

  user_node_t* curr2 = server_info->chat_users->first_user;

  // Also, remove all players from this game's list of players.
  while (curr2 != NULL) {
    user_node_t *temp = curr2;
    remove_user(curr2->socket_fd);
    curr2 = temp->next;
  }

  free(buf);
  free(winner_of_game);
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

/********************************************
 * Thread Worker Functions (Core Functions)
 *******************************************/

/**
 * Start the game by picking the first connected user to be the host, getting a secret word from 
 * the host, picking the first asker (as the player that connected to the game second fastest), 
 * and creating threads for each player to be able to forward messages between all of them and to 
 * run the game logic.
 */
void* start_game(void* args) {
  server_info_t* server_info = (server_info_t *) args;

  // Pick the first host to start the game.
  pthread_mutex_lock(&server_info_global_lock);
  server_info_global->curr_host = server_info_global->chat_users->first_user;
  pthread_mutex_unlock(&server_info_global_lock);

  char* pick_secret_msg = "You are the host. Pick your secret word.";

  pthread_mutex_lock(&server_info_global_lock);
  // Send a message to the first host to pick a secret word.
  send_message_to_player("Server", pick_secret_msg, server_info_global->curr_host->socket_fd);

  // Receive the secret word from the host.
  user_info_t* user_info = receive_message(server_info_global->curr_host->socket_fd);

  user_node_t* current = server_info_global->chat_users->first_user;
  pthread_mutex_unlock(&server_info_global_lock);

  char* start_game_msg = "The game has started. Wait for your turn to ask a question about the "
                         "secret word.";

  // Tell non-host players that the game has started and to wait for their turn to ask the host 
  // a question.
  while (current != NULL) {
    pthread_mutex_lock(&server_info_global_lock);
    if (current->socket_fd != server_info_global->curr_host->socket_fd) {
      send_message_to_player("Server", start_game_msg, current->socket_fd);
    }

    current = current->next;
    pthread_mutex_unlock(&server_info_global_lock);
  }
  
  // Save the secret word.
  pthread_mutex_lock(&server_info_global_lock);
  server_info_global->secret_word = strdup(user_info->message);

  // Set the first asker (as the next player after the host in the linked list).
  server_info_global->curr_asker = server_info_global->curr_host->next;
  pthread_mutex_unlock(&server_info_global_lock);

  // Tell current asker to send a question.
  char* start_asking_msg = "It is your turn to ask the host a Yes/No question about the secret word.";

  pthread_mutex_lock(&server_info_global_lock);
  send_message_to_player("Server", start_asking_msg, server_info_global->curr_asker->socket_fd);
  pthread_mutex_unlock(&server_info_global_lock);

  // Loop through list of players, and create a thread for each so that they can start 
  // communicating w/ e/o.
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

  // Create a local copy of server_info to ONLY read data from the struct w/o needing to lock.
  pthread_mutex_lock(&server_info_global_lock);
  server_info_t* server_info = server_info_global;
  pthread_mutex_unlock(&server_info_global_lock);

  while (true) {
    // Read a message from the player.
    user_info_t* user_info = receive_message(user_socket_fd);

    // Validate players' guesses if the round is in the guessing stage.
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

    // Remove the user if there's some error when trying to receive a message from it or the user 
    // is quitting the game.
    if (user_info == NULL || strcmp(user_info->message, "quit") == 0) {
      // Remove the user from the game's list of players.
      remove_user(user_socket_fd);
    
      // Close server's end of the socket.
      close(user_socket_fd);
      break;
    } else {
      pthread_mutex_lock(&server_info_global_lock);
      // Only don't forward a user's message to all users if the message is the secret word.
      if (!server_info_global->is_receiving_secret_word && !server_info_global->is_guessing && 
          ((user_socket_fd == server_info_global->curr_asker->socket_fd) || 
           (user_socket_fd == server_info_global->curr_host->socket_fd))) {
        // Forward the message to everyone.
        send_message_to_everyone(server_info, user_info);
      }

      // Tell users that try to send messages when it's not their turn to wait.
      if (!server_info_global->is_receiving_secret_word && !server_info_global->is_guessing && 
          !server_info_global->end_game) {
        if ((user_socket_fd != server_info_global->curr_asker->socket_fd) && 
            (user_socket_fd != server_info_global->curr_host->socket_fd)) {
          send_message_to_player("Server", "It is not your turn yet. Please wait.", 
                                 user_socket_fd);
        }
      }

      // At this point, the secret word should be received, so reset that state.
      if (server_info_global->is_receiving_secret_word) {
        server_info_global->is_receiving_secret_word = false;
      }
      pthread_mutex_unlock(&server_info_global_lock);

      // Once the current host has answered the question, change the current asker.
      // NOTE: The current host should always be sending a Y/N answer.
      if (user_socket_fd == server_info->curr_host->socket_fd &&
          (strcmp(user_info->message, "y") == 0 || strcmp(user_info->message, "n") == 0)) {
        pthread_mutex_lock(&server_info_global_lock);
        // Update the number of questions the host has answered.
        server_info_global->curr_question++;
        pthread_mutex_unlock(&server_info_global_lock);

        // Update the current asker.
        update_asker();

        pthread_mutex_lock(&server_info_global_lock);
        // If all questions in a round have been answered, proceed to guessing the secret word.
        if (server_info_global->curr_question == server_info_global->max_questions) {
          server_info_global->is_guessing = true; // It is time for guessing.
          user_node_t* current = server_info->chat_users->first_user;

          // Send that message to all non-host players, who can begin making their guess.
          while (current != NULL) {
            if (current != server_info->curr_host) {
              send_message_to_player("Server", 
                                     "It is time to make your guess for the secret word.", 
                                     current->socket_fd);
            }

            current = current->next;
          }
        }
        pthread_mutex_unlock(&server_info_global_lock);
      }
      
      // Do setup for the next round once the secret word has been guessed and there is still a 
      // player that hasn't been the host yet.
      pthread_mutex_lock(&server_info_global_lock);
      if (server_info_global->guessed_secret_word && server_info_global->curr_host->next != NULL) {
        pthread_mutex_unlock(&server_info_global_lock);
        // Update the host and first guesser of the next round, and get ready to read in the next
        // secret word.
        set_up_for_next_round();
      } else if (server_info_global->guessed_secret_word && 
                 server_info_global->curr_host->next == NULL) { // Done with the game.
        // Announce the winner of the game, print each player's score privately, and disconenct
        // everyone at the end.
        end_game(server_info);
      }
      
      // Every time a player becomes the current asker, tell the player to send a question.
      if (server_info_global->asker_updated && 
          server_info->curr_question < server_info->max_questions && 
          !server_info_global->is_receiving_secret_word) {
        send_message_to_player("Server", 
                               "It is your turn to ask the host a Yes/No question about the secret"
                               " word.", 
                               server_info_global->curr_asker->socket_fd);
        // Reset the state of the asker being updated.
        server_info_global->asker_updated = false;
      }

      // Every time a player becomes the new host, tell the player to set a secret word.
      if (server_info_global->host_updated) {
        send_message_to_player("Server", "You are the host. Pick your secret word.", 
                               server_info_global->curr_host->socket_fd);

        // Reset the state of the host being updated.
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
  // Open a server socket.
  unsigned short port = 0;
  int server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1) {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // Start listening for connections, with a maximum of one queued connection.
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
      // Indicate that the game has started once there are at least 2 players connected.
      server_info_global->is_game_initialized = true;
      pthread_mutex_unlock(&server_info_global_lock);

      // Create thread to start game.
      pthread_t thread;
      pthread_create(&thread, NULL, start_game, server_info_global);
    }
    pthread_mutex_unlock(&server_info_global_lock);

    // Accept connection from user.
    int client_socket_fd = server_socket_accept(server_socket_fd); 
    pthread_mutex_lock(&server_info_global_lock);
    // Remember the socket fd of who just connected to use later.
    server_info_global->connecting_user_socket_fd = client_socket_fd;
    pthread_mutex_unlock(&server_info_global_lock);

    char* welcome_msg = "Welcome to the Guessing Secret Word game!\n Each player will "
      "take turn to be the host and pick a secret word.\n Other players will take turn to ask "
      "yes/no questions to guess the secret word.\n Whoever makes the most correct guesses will "
      "be the winner!\n";

    // Send a welcome message with the game instructions to the connecting player.
    send_message_to_player("Server", welcome_msg, client_socket_fd);

    // Add new player to list of players.
    add_player_to_list(users, client_socket_fd);

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
