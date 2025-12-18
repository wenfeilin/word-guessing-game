# Word Guessing Game

Created by Linh Vu, Wenfei Lin, & Connor Durkin. 
Last updated on December 17, 2025.

## Game Basics

The basic idea of the game is to correctly guess a secret word that has been set by another player. Once a guess matches the secret word, the player that guessed correctly receives 1 point. The player with the most points at the end of the game is the winner. 

Players swap between 2 roles: 
- Host: Inputs a secret word for other players to guess. 
- Guesser: Inputs questions and guesses about the secret word.

Every game consists of 2 phases: 
- QnA Rounds: Each player will ask the host a question about the secret word. Other players cannot ask a question until it is their turn. The host will answer each question with a yes or no.
- Guessing Free-For-All: All players can input their guesses for the secret word in a free-for-all.

## How to Run

```bash
# 1. Make the project.
$ make

# 2. In a separate terminal, run the server.
$ ./server
  Server running on port [port-number]

# 3. For each player, in a separate terminal, run the client by using the port number outputted from running the server.
$ ./client [username] localhost [port-number]
  [Welcome message with game instructions]
```

## How to Play
Once 2 players have been connected, the game will automatically start, and the player that joined first will become the host.

In these examples, the players joined in the following order: `user1`, `user2`, and `user3`. This means that the host is `user1`, the first guesser of the first round is `user2`, and the second guesser of the first round is `user3 `. 

### For Each Round

1. The host first sets the secret word. This will be the word that the rest of the players try to guess after asking questions about it.

    Host: (upon joining game)
  
    <img width="572" height="119" alt="image" src="https://github.com/user-attachments/assets/7de732d9-db87-4441-8059-cf101d508fb6" />

    Guessers: (upon joining game)

    <img width="569" height="105" alt="image" src="https://github.com/user-attachments/assets/ce1a1a56-759c-4a29-9124-cf04100bfebb" />

    ---

    Host: (upon setting a secret word)
  
    <img width="572" height="138" alt="image" src="https://github.com/user-attachments/assets/fe6dc8f3-57bb-4610-955f-a1893718416f" />

    Guesser (user 2): (player who gets to ask the first question)

    <img width="637" height="137" alt="image" src="https://github.com/user-attachments/assets/db7d65a3-8854-4f96-9700-94ebf4e30d0f" />


    Guesser (user 3): (player who doesn't get to ask the first question)

    <img width="639" height="126" alt="image" src="https://github.com/user-attachments/assets/b5c46762-0099-4b85-a7e1-30eec913f98d" />

2. The QnA round then begins. All other players will ask, taking turns in the order that they connected to the sever, a question about the secret word. After a player asks the host a yes/no question, the host should respond with yes or no. If it's not a player's turn to ask a question, their question won't be sent to everyone else, and the server will indicate that.

    Guesser (user 3): (sending a question when it's not their turn)

    <img width="638" height="160" alt="image" src="https://github.com/user-attachments/assets/10c24730-8c7a-409d-9dee-8a08aa5a7aa0" />

    Guesser (user 2): (sending a question when it's their turn)

    <img width="654" height="170" alt="image" src="https://github.com/user-attachments/assets/40324de2-9825-44b8-92d3-3a464108814d" />
    
    Host:

    <img width="570" height="156" alt="image" src="https://github.com/user-attachments/assets/97529573-6bde-46cc-8186-613c2192a28b" />
    
    ---
   
    Answering the question:

    Host: (answering "yes" to the question)

    <img width="327" height="68" alt="image" src="https://github.com/user-attachments/assets/4ab68820-6222-4684-9fc4-4dd4770119a2" />

    Guessers:

    <img width="328" height="51" alt="image" src="https://github.com/user-attachments/assets/0366009b-e05e-41db-ab73-5bf882b93ece" />

3. After some of questions are asked (defined by the variable `max_questions`), the guessing free-for-all will begin. Players can enter in their guesses in any order. The guess will be checked against the secret word in a case-insensitive manner. Once a guess matches the secret word, the player who guessed correctly will receive 1 point. Then, the round ends, so the host switches to the next player. The new host selects a secret word, and the game continues onward until all players have been the host once.

    Note: `max_questions` is currently set to 2, which means that each round only allows for 2 questions to be asked/answered in total. To change this, go to line 797 on `server.c` and change `server_info_global->max_questions` to the number of questions you prefer to have asked/answered in each round.

    Guesser (user 2): (making the wrong guess)

    <img width="423" height="71" alt="image" src="https://github.com/user-attachments/assets/a9121da1-69b6-440e-a323-59804a0ba1b3" />

    Guesser (user 3): (making the right guess)

    <img width="418" height="56" alt="image" src="https://github.com/user-attachments/assets/cc25ec4c-644d-4f5f-af44-fe40264c6e69" />

    Guesser(user 2): (becomes the host of the next round after user2 guesses correctly)

    <img width="357" height="52" alt="image" src="https://github.com/user-attachments/assets/22279075-5a09-49ab-9d46-0ecbb9020ad6" />

### At End of Game

Once the game ends, the final winner of the game is announced, and each player receives their individual score privately.

  User 1:

  <img width="342" height="69" alt="image" src="https://github.com/user-attachments/assets/e2665c59-b8d2-44f6-bef6-fceefc3d8b7d" />

  User 2:

  <img width="336" height="71" alt="image" src="https://github.com/user-attachments/assets/37eb63c2-b61f-482d-992d-3921c370f63a" />

  User 3:

  <img width="353" height="68" alt="image" src="https://github.com/user-attachments/assets/542b69a4-dc59-453c-9e07-e5aec089a3d1" />

  Note: The players can then quit the game by typing `quit`.
