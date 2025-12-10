#include "message.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Send a across a socket with a header that includes the message length.
int send_message(int fd, user_info_t* user_info) {
  // If the message or user info is NULL, set errno to EINVAL and return an error
  if (user_info == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (user_info->message == NULL) {
    errno = EINVAL;
    return -1;
  }

  // First, send the length of the message in a size_t
  size_t message_len = strlen(user_info->message);
  if (write(fd, &message_len, sizeof(size_t)) != sizeof(size_t)) {
    // Writing failed, so return an error
    return -1;
  }

  // Now we can send the message. Loop until the entire message has been written.
  size_t bytes_written = 0;
  while (bytes_written < message_len) {
    // Try to write the entire remaining message
    ssize_t rc = write(fd, user_info->message + bytes_written, message_len - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }

  // Then, send the length of the username in a size_t
  size_t username_len = strlen(user_info->username);
  if (write(fd, &username_len, sizeof(size_t)) != sizeof(size_t)) {
    // Writing failed, so return an error
    return -1;
  }

  // Now we can send the username. Loop until the entire username has been written.
  bytes_written = 0;
  while (bytes_written < username_len) {
    // Try to write the entire remaining username
    ssize_t rc = write(fd, user_info->username + bytes_written, username_len - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }

  return 0;
}

// Receive a message from a socket and return the message string (which must be freed later)
user_info_t* receive_message(int fd) { // TO DO: send a struct to indicate whether msg is sent from server or not so we know whether to read once (from server) or twice (from other users)
  user_info_t* user_info = malloc(sizeof(user_info_t));

  // First try to read in the message length
  size_t message_len;
  if (read(fd, &message_len, sizeof(size_t)) != sizeof(size_t)) {
    // Reading failed. Return an error
    return NULL;
  }

  // Now make sure the message length is reasonable
  if (message_len > MAX_MESSAGE_LENGTH) {
    errno = EINVAL;
    return NULL;
  }

  // Allocate space for the message and a null terminator
  char* message_result = malloc(message_len + 1);

  // Try to read the message. Loop until the entire message has been read.
  size_t bytes_read = 0;
  while (bytes_read < message_len) {
    // Try to read the entire remaining message
    ssize_t rc = read(fd, message_result + bytes_read, message_len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0) {
      free(message_result);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }

  // Add a null terminator to the message
  message_result[message_len] = '\0';

  user_info->message = message_result;

  // Then, try to read in the username length
  size_t username_len;
  if (read(fd, &username_len, sizeof(size_t)) != sizeof(size_t)) {
    // Reading failed. Return an error
    return NULL;
  }

  // Now make sure the username length is reasonable
  if (username_len > MAX_MESSAGE_LENGTH) {
    errno = EINVAL;
    return NULL;
  }

  // Allocate space for the username and a null terminator
  char* username_result = malloc(username_len + 1);

  // Try to read the username. Loop until the entire username has been read.
  bytes_read = 0;
  while (bytes_read < username_len) {
    // Try to read the entire remaining username
    ssize_t rc = read(fd, username_result + bytes_read, username_len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0) {
      free(username_result);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }

  // Add a null terminator to the username
  username_result[username_len] = '\0';

  user_info->username = username_result;

  return user_info;
}
