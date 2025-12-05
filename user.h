#ifndef USER_H
#define USER_H

/**
 * Structure to store the username of a user and the message a user is sending.
 */
typedef struct user_info {
  char* message;
  char* username;
} user_info_t;
#endif
