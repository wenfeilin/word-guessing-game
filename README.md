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

```
# 1. Make the project.
$ make

# 2. In a separate terminal, run the server.
$ ./server
  Server running on port [port-number]

# 3. For each player, in a separate terminal, run the client by using the port number outputted from runner the server.
$ ./client [username] localhost [port-number]
  # Welcome message with game instructions should appear upon connecting to the server.
```

## How to Play
Once 2 players have been connected, the game will automatically start, and the player that joined first will become the host.

For each round:
1. The host first sets the secret word. This will be the word that the rest of the players try to guess after asking questions about it.

Input: meep
Output (message from server): 

2. The QnA round then begins. All other players will ask, taking turns in the order that they connected to the sever, a question about the secret word. After a player asks the host a yes/no question, the host should respond with yes or no. If it's not a player's turn to ask a question, their question won't be sent to everyone else, and the server will indicate that.

Input: 
Output (message from server): 


Note: 

3. After some of questions is asked (defined by the variable `max_questions`), the guessing free-for-all will begin. Players can enter in their guesses in any order. The guess will be checked against the secret word in a case-insensitive manner. Once a guess matches the secret word, the player who guessed correctly will receive 1 point. Then, the round ends, so the host switches to the next player. The new host selects a secret word, and the game continues onward until all players have been the host once.

Input: 
Output (message from server): 

4. Once the game ends, the final winner of the game is announced, and each player receives their individual score privately.
 
Input: 
Output (message from server): 
