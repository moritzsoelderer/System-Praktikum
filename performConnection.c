#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <signal.h>
#include <sys/wait.h>
#include <poll.h>

#include "fetch.h"

int recieveMessage(int sock, char* response);
int prologPhase(int sock, char* gameId, char* gkn, int buffer);
int getPlayerInfo(int sock, struct shmem* sharedMem);
int moveSeq(int sock, struct shmem* sharedMem);

int performConnection(int sock, char* gameId,char* gkn, int buffer, struct shmem* sharedMem, int pipe){
    
    if(prologPhase(sock, gameId, gkn, buffer) != 0){
        return EXIT_FAILURE;
    }

    //player Info fetch (number, name, ready) + Total Players
    if(getPlayerInfo(sock, sharedMem) != 0){
        shmdt(sharedMem->players);
        return EXIT_FAILURE;
    }

    char response[buffer];
    
    do{
        memset(response, 0, buffer);
        if(recieveMessage(sock, response) != 0){
            return EXIT_FAILURE;
        }
        //positive responses
        if(strcmp(response, "+ WAIT\n") == 0){
            send(sock, "OKWAIT\n", strlen("OKWAIT\n"), 0);
        }
        else if(strncmp(response, "+ MOVE ", strlen("+ MOVE ")) ==  0){
            moveSeq(sock, sharedMem);
        }
        else if(strcmp(response, "+ OKTHINK\n") == 0){
            sharedMem->flag = 1;
            kill(getppid(),SIGUSR1);
            char message[buffer];
            memset(message, 0, buffer);
            read(pipe, message, buffer);
            sharedMem->flag = 0;
            puts(message);
            send(sock, message, strlen(message), 0);
        }
        else if(strcmp(response, "+ GAMEOVER\n") == 0){
            do{
                memset(response, 0, buffer);
                recieveMessage(sock, response);
            }
            while(strcmp(response, "+ ENDPIECELIST\n") != 0);
            
            int winners[sharedMem->playerCounter];
            memset(winners, 0, sharedMem->playerCounter);
            int j = 0;
            for(int i = 0; i< sharedMem->playerCounter; i++){
                memset(response, 0, buffer);
                recieveMessage(sock, response);
                char won[4] = "";
                int playerNumber;
                sscanf(response, "PLAYER%iWON %s\n",&playerNumber, won);
                if(strcmp(won, "Yes") == 0){ //valgrind fehler
                    winners[j] = playerNumber;
                    j++;
                } 
            }
        }
    }
    while(strcmp(response, "+ QUIT\n") != 0);
    shmdt(sharedMem->players);
    return EXIT_SUCCESS;
    
}

int getPlayerInfo(int sock, struct shmem* sharedMem){
    int buffer = 128;
    char response[buffer];
    memset(response, 0, buffer);

    //Client player number + name fetch
    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    int playerNumber = 0;
    char playerName[31];
    sscanf(response, "+ YOU %i %s\n", &playerNumber, playerName); //player name isnt stored properly!!!
    memset(response, 0, buffer);

    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    sscanf(response, "+ TOTAL %i\n", &sharedMem->playerCounter);
    
    //player sharedMem segment
    if((sharedMem->playerShmid = shmget(IPC_PRIVATE, sizeof(struct player)*sharedMem->playerCounter,IPC_CREAT | 0600)) == -1){
        perror("SHM-Erstellung fehlgeschlagen.");
        return EXIT_FAILURE;
    }

    if((sharedMem->players = shmat(sharedMem->playerShmid,NULL, 0)) == (void*)-1){
        perror("SHM-Anbindung fehlgeschlagen.");
        return EXIT_FAILURE;
    }

    //storing playernumber and playername of client
    strcpy(sharedMem->players->playerName, playerName);
    sharedMem->players->playerNumber = playerNumber;

    int j = 1;
    for(int i = 0; i<sharedMem->playerCounter; i++){
        if(i != sharedMem->players->playerNumber){
            memset(response, 0, buffer);
            if(recieveMessage(sock, response) == -1){
                return EXIT_FAILURE;
            }
            sscanf(response, "+ %i", &sharedMem->players[j].playerNumber);
            strncpy((sharedMem->players+ j*sizeof(struct player))->playerName, (response+4), strlen(response) -3 -3);
            (sharedMem->players+ j*sizeof(struct player))->ready = atoi(response+strlen(response)-3);  
            j++;
        }
    }
    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int prologPhase(int sock, char* gameId, char* gkn, int buffer){
    //PROLOG-PHASE
    char response[buffer];
    char message[buffer];
    memset(response, 0, buffer);
    memset(message,0,buffer);

    //GameServer Version
    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    puts("Der Game-Server akzeptiert Verbindungen.");
    int serverVersion = 0;
    sscanf(response, "+ MNM Gameserver v%i.", &serverVersion);
    sprintf(message, "VERSION %i.3\n", serverVersion);
    send(sock, message, strlen(message),0);
    memset(message,0,buffer);
    memset(response, 0, buffer);

    //daily message
    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    
    //Game-ID transmission
    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    sprintf(message, "ID %s\n", gameId);
    send(sock, message, strlen(message),0);
    memset(message,0,buffer);
    memset(response, 0, buffer);

    //Game-Kind-Name check
    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    char gknFromServer[24];
    sscanf(response, "+ PLAYING %s\n", gknFromServer);
    if(strcmp(gkn, gknFromServer) != 0){
        puts("GameKindName stimmt nicht überein");
        return EXIT_FAILURE;
    }
    //Game Name
    memset(response, 0, buffer);
    if(recieveMessage(sock, response) == -1){
        return EXIT_FAILURE;
    }
    strcpy(message, "PLAYER\n");
    send(sock, message, strlen(message),0);

    printf("Verbindung wurde hergestellt. Wir spielen ");
    printf("%s!\n", gknFromServer);

    return EXIT_SUCCESS;
}

int moveSeq(int sock, struct shmem* sharedMem){

    int buffer = 64;
    char response[buffer];
    memset(response, 0, buffer);

    //number of pieces to capture fetch
    if(recieveMessage(sock,response) != 0){
        return EXIT_FAILURE;
    }
    sscanf(response, "+ CAPTURE %u\n", &sharedMem->players->toCapture);
    memset(response, 0, buffer);

    //number of pieces per player fetch
    if(recieveMessage(sock,response) != 0){
        return EXIT_FAILURE;
    }
    unsigned int numPieces = 0;
    int x;
    sscanf(response, "+ PIECELIST %i,%u\n", &x, &numPieces); //x nur als Platzhalter
    memset(response, 0, buffer);

    //number of pieces per player assignment to players
    for(int i = 0; i<sharedMem->playerCounter; i++){
        sharedMem->players[i].numPieces = numPieces;
    }

    printf("capturedPieces: %i\n", sharedMem->players[0].capturedPieces);
    printf("numPieces: %i\n", sharedMem->players[0].numPieces);

    //recieving first piece
    if(recieveMessage(sock,response) != 0){
        return EXIT_FAILURE;
    }

    sharedMem->players->availablePieces = 0;
    sharedMem->players->capturedPieces = 0;
    memset(sharedMem->board, 0, sizeof(sharedMem->board));
    //fetching and storing location + recieving rest of pieces
    while(strcmp(response, "+ ENDPIECELIST\n")!= 0){
        char position[3] = {0};
        int x,y = 0;
        sscanf(response, "+ PIECE%i.%i %s\n", &x, &y, position);
        if(strlen(position) > 1){
            int i,j = 0;
            transformCoordinates(position, &i,&j);
            sharedMem->board[i][j] = x+1; //+1 weil array mit 0 initialisiert wird
        }
        if(x == sharedMem->players[0].playerNumber){
            if(strcmp(position, "A") == 0){
                sharedMem->players[0].availablePieces++;
            }
            else if(strcmp(position, "C") == 0){
                puts("im if statement");
                sharedMem->players[0].capturedPieces++;
            }
        }
        
        if(recieveMessage(sock,response) != 0){
            return EXIT_FAILURE;
        }
    }

    send(sock, "THINKING\n", strlen("THINKING\n"), 0);
    
    //printing Board
    printBoard(sharedMem->board);

    return EXIT_SUCCESS;
}

int handleNegativeResponses(char* response){
    if(strcmp(response, "- Internal error. Sorry & Bye\n") == 0){
        puts("Es liegt ein server-internes Problem vor. Versuchen Sie es später erneut.");
        return EXIT_SUCCESS;
    }
    else if(strcmp(response, "- TIMEOUT Be faster next time\n") == 0){
        puts("Der Server meldet ein Timeout, weil der Client zu langsam war.");
        return EXIT_SUCCESS;
    }
    else if(strcmp(response, "- No free player\n") == 0){
        puts("Es ist kein Spieler frei.");
        return EXIT_SUCCESS;
    }
    printf("%s",response);
    return EXIT_FAILURE;
}

int recieveMessage(int sock, char* response){
    int i = 0;
    do{
        if(recv(sock,(response+i*sizeof(char)),1, 0) <= 0){
            return -1;
        }
        i++;
    }
    while(response[i-1]!= '\n');
    if(response[0] == '-'){
        handleNegativeResponses(response);
        return EXIT_FAILURE;
    }
    return 0;
}