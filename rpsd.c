#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include "network.h"

#define BUFSIZE 256
#define MAX_PLAYERS 100

char *active_players[MAX_PLAYERS];

typedef struct { //Player Struct
    int sock; //File descriptor for destination socket
    char name[100]; //Name of the player
    char move[20]; //Move that the player is going to use
} Player;

int is_active(const char *name) { //Checks active players array and returns 1 if the name matches an active player
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (active_players[i] && strcmp(active_players[i], name) == 0)
            return 1;
    }
    return 0;
}

void add_active(const char *name) { //Adds an active player to the active players list
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (active_players[i] == NULL) {
            active_players[i] = strdup(name);
            return;
        }
    }
}

void remove_active(const char *name) { //If a player is no longer active, remove them from the list
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (active_players[i] && strcmp(active_players[i], name) == 0) {
            free(active_players[i]);
            active_players[i] = NULL;
            return;
        }
    }
}

void send_msg(int sock, const char *msg) { //Writes the message sent by the player 
    write(sock, msg, strlen(msg));
}

int read_msg(int sock, char *buf, int bufsize) { //Reads messages sent in by the players
    int total = 0;
    while (total < bufsize - 1) {
        char byte;
        int n = read(sock, &byte, 1);
        if (n <= 0){
             return -1; //Empty message
        }
        buf[total++] = byte;
        if (total >= 2 && buf[total-2] == '|' && buf[total-1] == '|') {
            buf[total] = '\0';
            return total;
        }
    }
    return -1; //If total >= bufsize then return error number
}

const char* determine_result(const char *m1, const char *m2) { //Returns the char based on the result of the moves
    if (strcmp(m1, m2) == 0) return "D"; //If the inputs are equal then return a Draw
    if ((strcmp(m1, "ROCK") == 0 && strcmp(m2, "SCISSORS") == 0) ||
        (strcmp(m1, "PAPER") == 0 && strcmp(m2, "ROCK") == 0) ||
        (strcmp(m1, "SCISSORS") == 0 && strcmp(m2, "PAPER") == 0)) { //All win conditions for the given connected player
        return "W";
    }
    return "L"; //Returns L if a win condition is not met
}

void handle_game(Player p1, Player p2) { //Runs while a game is running goes through the game flow from welcome message, to processing moves to the continue/quit input
    int run = 1;
    while (run){
    printf("Game started: %s vs %s\n", p1.name, p2.name); //Printf messages to log activity, game started logged in rpsd STDOUT

    //Send begin messages
    char msg1[BUFSIZE];
    char msg2[BUFSIZE];
    snprintf(msg1, BUFSIZE, "B|%s||", p2.name);
    snprintf(msg2, BUFSIZE, "B|%s||", p1.name);
    send_msg(p1.sock, msg1);
    send_msg(p2.sock, msg2);

    //Read move from Player 1
    char buf1[BUFSIZE];
    if (read_msg(p1.sock, buf1, BUFSIZE) < 0 || strncmp(buf1, "M|", 2) != 0) { //Handles forfeit logic
        send_msg(p2.sock, "R|F|||");
        close(p1.sock);
        close(p2.sock);
        printf("Forfeit by %s\n", p1.name);
        remove_active(p1.name);
        exit(EXIT_SUCCESS);
    }
    sscanf(buf1, "M|%[^|]||", p1.move);
    printf("%s played %s\n", p1.name, p1.move); //Log p1 move to rpsd STDOUT


    //Read move from Player 2
    char buf2[BUFSIZE];
    if (read_msg(p2.sock, buf2, BUFSIZE) < 0 || strncmp(buf2, "M|", 2) != 0) { //Handles forfeit logic
        send_msg(p1.sock, "R|F|||");
        close(p1.sock);
        close(p2.sock);
        printf("Forfeit by %s\n", p2.name);
        remove_active(p2.name);
        exit(EXIT_SUCCESS);
    }
    sscanf(buf2, "M|%[^|]||", p2.move);
    printf("%s played %s\n", p2.name, p2.move); //Log p2 move to rpsd STDOUT


    //Send result messages
    const char *r1 = determine_result(p1.move, p2.move);
    const char *r2 = determine_result(p2.move, p1.move);

    char res1[BUFSIZE];
    char res2[BUFSIZE];
    snprintf(res1, BUFSIZE, "R|%s|%s||", r1, p2.move);
    snprintf(res2, BUFSIZE, "R|%s|%s||", r2, p1.move);
    send_msg(p1.sock, res1);
    send_msg(p2.sock, res2);
    if(strcmp(r1, "W") == 0){ //Log win message in rpsd STDOUT
        printf("%s Wins!\n", p1.name);
    }
    else{
        printf("%s Wins!\n", p2.name);
    }

    char p1_choice;
    char p2_choice;

    //Loop to read continue or quit from each player
    for (int i = 0; i < 2; ++i) {
        char choice;
        int n;
        if (i == 0){
            n = read(p1.sock, &choice, 1);
        }
        else{
            n = read(p2.sock, &choice, 1);
        }
        if (n <= 0 || (choice != 'C' && choice != 'Q')) {
            remove_active(p1.name);
            remove_active(p2.name);
            close(p1.sock);
            close(p2.sock);
            exit(EXIT_SUCCESS);
        }
        if (choice == 'Q') { //
            printf("%s quit\n", i == 0 ? p1.name : p2.name);
            remove_active(p1.name);
            remove_active(p2.name);
            close(p1.sock);
            close(p2.sock);
            exit(EXIT_SUCCESS);
        }
        if (i == 0) {
               p1_choice = choice;
           } else {
               p2_choice = choice;
           }
    }
    if (p1_choice == 'C' && p2_choice == 'C') { //Checks if both players have continued
        printf("Both players chose to continue. Starting rematch: %s vs %s\n", p1.name, p2.name);
        continue;
    } else {
        remove_active(p1.name);
        remove_active(p2.name);
        close(p1.sock);
        close(p2.sock);
        exit(EXIT_SUCCESS);
    }
}
    
}

int main(int argc, char *argv[]) {
    signal(SIGCHLD, SIG_IGN); //Stops Zombies

    if (argc != 2) { //Make sure the arg count is equal to 2 since it should just be the program name and port name, returns error if argc !=2
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listener = open_listener(argv[1], 1); //Opens a listener for the port, returns an error if does not work for any reason
    if (listener < 0) {
        perror("open_listener");
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %s\n", argv[1]); //Confirmation message that server is running
    int run = 1;
    while (run) { //Will run until game ends for whatever reason.
        Player p1;
        Player p2;

        for (int i = 0; i < 2; ++i) {
            int sock = accept(listener, NULL, NULL);
            if (sock < 0){
                continue;
            }

            char buf[BUFSIZE];
            if (read_msg(sock, buf, BUFSIZE) < 0 || strncmp(buf, "P|", 2) != 0) {
                close(sock);
                --i;
                continue;
            }

            char name[BUFSIZE];
            sscanf(buf, "P|%[^|]||", name);

            if (is_active(name)) { //If player is active and name is scanned then send this loss message
                char reject_msg[BUFSIZE];
                snprintf(reject_msg, BUFSIZE, "R|L|Logged in||");
                send_msg(sock, reject_msg);
                close(sock);
                --i;
                continue;
            }

            Player *pl;
            if (i == 0){ //Logic determines whether pl points to player 1 or player 2
                pl = &p1;
            }
            else{
                pl = &p2;
            }
            pl->sock = sock;
            strncpy(pl->name, name, sizeof(pl->name));
            add_active(name); //Adds player to active player list
            send_msg(sock, "W|1||");

            printf("Connected: %s\n", pl->name); //Logs player connection to rpsd STDOUT
        }

        // Fork to run game
        if (fork() == 0) {
            close(listener); //Child doesn't need listener
            handle_game(p1, p2);
            remove_active(p1.name);
            remove_active(p2.name);
        } else {
            //Parent closes sockets, child handles game
            close(p1.sock);
            close(p2.sock);
        }
    }

    return 0;
}

