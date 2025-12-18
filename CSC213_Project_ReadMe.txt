ReadMe file for CSC213 Project: Word Guessing Game.
Created by Linh Vu, Wenfei Lin, & Connor Durkin. 
Last updated on December 17 2025.

-- GAME BASICS --
The basic idea of the game is to correctly guess a secret word that has been set by another player. Once a guess matches the secret word, the player that guessed correctly receives 1 point. The player with the most points at the end of the game is the winner. 

Players swap between 2 roles: (1) Host: Inputs a secret word for other players to guess. (2) Guesser: Inputs questions and guesses about the secret word.

Every game consists of 2 phases: (1) QnA Rounds: Each player will ask the host a question about the secret word. Other players cannot ask a question until it is their turn. The host will answer each question with a yes or no. (2) Guessing Free-For-All: All players can input their guesses for the secret word in a free-for-all.

-- INSTRUCTIONS --
Note that we are using the term "clients" interchangeably with "players," the former being the more technical term.

(1) Run makefile.

Input: make
Output: 

(2) Establish the server.

Input: ./server localhost
Output: 

(3) Connect the clients to the server. Once 2 clients have been connected, the game will automatically start, and the first client will become the host.

Input: ./server user1 localhost ...
Output: 

(4) Set the secret word from the terminal window of the host. This will establish the secret word that is to be guessed during the first part of the game.

Input: meep
Output (message from server): 

(5) The QnA round will begin. Clients will ask, in the order that they connected to the sever, a question about the secret word. So, in each client terminal, you will be prompted to enter a question, and then you will switch over to the host terminal and answer the questions one-by-one. Be sure to ask a question that can be answered with yes/no as the host can only respond with yes/no. When it's not a client's turn to ask a question, they cannot send a message, and so you have to ask a question from the appropriate client terminal. There will be messages from the server to guide you along this process.

Input: 
Output (message from server): 

(6) The guessing free-for-all will begin. Clients can enter in their guesses in any order. The guess will be checked against the secret word in a case-insensitive manner. Once a guess matches the secret word, the client who guessed correctly will receive 1 point. Then the game moves onto the next part, so the host switches to the following client, and then they select another secret word, and the game continues onward until all clients have been the host.

Input: 
Output (message from server): 