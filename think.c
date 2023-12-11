#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>

#include "fetch.h"

#define MILL 15
#define BLOCKMILL 7
#define MILLNEXTROUND 5
#define ALIGNMENT 2

#define FOURWAYCORNER 2
#define THREEWAYCORNER 1

void capture(struct shmem* sharedMem, struct player* players, tupel* opponentBestMove );
void checkOrthogonalPlacing(int i, int j, int board[3][8], struct shmem* sharedMem, struct player* players);
int computeAbsoluteWeights(tupel clientReachables[], tupel opponentReachables[], int clientReachablesCounter, int opponentReachablesCounter);
int computeReachables(tupel reachables[], int board[3][8], int player, int* k);
int computeBestMove(tupel reachables[], int reachableCounter, tupel* bestMove);
void checkOrthogonal(int i, int j, int player, int* k, int board[3][8], tupel reachables[], int direction);
void checkRadial(int i, int j, int player, int* k, int board[3][8], tupel reachables[], int direction);

//TODO Spielelogik einrichten
//+ server responses schön ausgeben
//gute Ausgaben für Error Nachrichten vom Server implementieren
int think(struct shmem* sharedMem, int pipe, int buffer){
    struct player* players;

    if((players = shmat(sharedMem->playerShmid,NULL, 0)) == (void*)-1){
        return EXIT_FAILURE;
    }
    if(sharedMem->flag == 1){ //WICHTIG: Caputre funktioniert in der Move Sequenz noch nicht optimal
        sharedMem->flag = 0;
        //CAPTURE
        if(players->toCapture != 0){
            tupel opponentBestMove;
            capture(sharedMem,players,&opponentBestMove);
            char message[64];
            sprintf(message, "PLAY %s\n", opponentBestMove.x);
            write(pipe,message, strlen(message));
            return EXIT_SUCCESS;
        }
        
        //PLACE PIECES
        else if(players[0].availablePieces != 0){
            //max 46 reachables
            //field copy
            int board[3][8];
            memset(board, 0, 24*sizeof(int));
            //preset good places:
            for(int i = 1; i < 8; i+=2){
                board[0][i] += THREEWAYCORNER;
                board[1][i] += FOURWAYCORNER;
                board[2][i] += THREEWAYCORNER;
            }
            //orthogonal
            for(int i = 0; i < 3; i++){
                for(int j = 0; j<=6; j+=2){
                    checkOrthogonalPlacing(i,j,board,sharedMem,players);
                }
            }

            //radial
            for(int j = 1; j < 8; j+=2){
                int x = 0;
                int y = 0;
                int opponentCounter = 0;
                int clientCounter = 0;
                for(int i = 0; i<3; i++){
                    if(sharedMem->board[i][j]-1 == players[1].playerNumber){
                        opponentCounter++;
                        board[i][j] = -100;
                    }
                    else if(sharedMem->board[i][j] == 0){
                        x = i;
                        y = j;
                    }
                    else if(sharedMem->board[i][j]-1 == players[0].playerNumber){
                        clientCounter++;
                        board[i][j] = -100;
                    }
                }
                if(opponentCounter - clientCounter == 2){
                    board[x][y] += BLOCKMILL;
                }
                if(clientCounter - opponentCounter == 2){
                    board[x][y] += MILL;
                }
            }
            
            //search for highest value
            int xMax = 0;
            int yMax = 0;
            int valMax = 0;
            for(int i = 0; i < 3; i++){
                for(int j = 0; j < 8; j++){
                    if(board[i][j] >= valMax){
                        valMax = board[i][j];
                        xMax = i;
                        yMax = j;
                    }
                }
            }
            char message[64];
            char position[3];
            transformCoordinatesInverse(xMax,yMax,position);
            sprintf(message, "PLAY %s\n",position);
            write(pipe,message, strlen(message));
        }
        //MOVE PIECES ONLY 3 PIECES LEFT
        else if(players[0].numPieces - players[0].capturedPieces <= 3){
            puts("only 3 pieces left!");
            int x,y = 0;
            int a,b = 0;
            for(int i = 0; i < 3; i++){
                for(int j = 0; j< 8; j++){
                    if(sharedMem->board[i][j] == players[0].playerNumber+1){
                        a = i;
                        b = j;
                    }
                    if(sharedMem->board[i][j] == 0){
                        x = i;
                        y = j;
                    }
                }
            }
            char dest[3];
            char start[3];
            char message[64];
            transformCoordinatesInverse(a,b, start);
            transformCoordinatesInverse(x,y, dest);
            sprintf(message, "PLAY %s:%s\n",start, dest);
            write(pipe,message, strlen(message));
            return EXIT_SUCCESS;
        }
        //MOVE PIECES
        else if(players[0].numPieces - players[0].capturedPieces > 3){
            tupel clientReachables[24]; //max reachable fields are 24
            memset(clientReachables, 0, sizeof(tupel)*24);
            tupel opponentReachables[24]; //max reachable fields are 24
            memset(opponentReachables, 0, sizeof(tupel)*24);

            int clientReachableCounter;
            int opponentReachableCounter;
            computeReachables(clientReachables, sharedMem->board, players[0].playerNumber, &clientReachableCounter);
            computeReachables(opponentReachables, sharedMem->board, players[1].playerNumber, &opponentReachableCounter);
            computeAbsoluteWeights(clientReachables, opponentReachables, clientReachableCounter, opponentReachableCounter);
            
            tupel finalBestMove;
            finalBestMove.weight = -10;
            int currentBestMoveIndex = 0;
            for(int i = 0; i<clientReachableCounter; i++){
                tupel clientReachables1[24]; //max reachable fields are 24
                memset(clientReachables1, 0, sizeof(tupel)*24);
                tupel opponentReachables1[24]; //max reachable fields are 24
                memset(opponentReachables1, 0, sizeof(tupel)*24);

                int clientReachableCounter1, opponentReachableCounter1 = 0;
                int board[3][8] = {{0}};
                memcpy(board, sharedMem->board, 24*sizeof(int));

                //simulate Move
                int x,y = 0;
                transformCoordinates(clientReachables[i].x, &x,&y);
                int player = board[x][y];
                board[x][y] = 0;

                //simulate Capture if necessary
                if(clientReachables[i].weight > 14){
                    tupel opponentBestMove;
                    capture(sharedMem,players,&opponentBestMove);
                    int x;
                    int y;
                    transformCoordinates(opponentBestMove.x, &x, &y);
                    sharedMem->board[x][y] = 0;
                    printBoard(sharedMem->board);
                }
                

                transformCoordinates(clientReachables[i].y, &x,&y);
                board[x][y] = player;


                //simulate opponents move
                tupel opponentBestMove;
                memset(opponentReachables, 0, opponentReachableCounter*sizeof(tupel));
                computeReachables(opponentReachables, board, players[1].playerNumber, &opponentReachableCounter);
                computeBestMove(opponentReachables, opponentReachableCounter, &opponentBestMove);
                transformCoordinates(opponentBestMove.x, &x,&y);
                player = board[x][y];
                board[x][y] = 0;
                transformCoordinates(opponentBestMove.y, &x,&y);
                board[x][y] = player;

                //compute reachables with new hypothetical board
                computeReachables(clientReachables1, board, players[0].playerNumber, &clientReachableCounter1);
                computeReachables(opponentReachables1, board, players[1].playerNumber, &opponentReachableCounter1);
                computeAbsoluteWeights(clientReachables1, opponentReachables1, clientReachableCounter1, opponentReachableCounter1);

                //add weight of current Move to all of the hypothetical moves
                for(int j = 0; j<clientReachableCounter1; j++){
                    clientReachables1[j].weight = (clientReachables1[j].weight/2-1) + clientReachables[i].weight; //regulating the weights (the earlier the better)
                }

                //compute best Move of the hypothetical moves (+ current move weight)
                tupel currentNextMove;
                computeBestMove(clientReachables1, clientReachableCounter1, &currentNextMove);

                //check if current next Move is better than those before and saving the index of the current move
                if(currentNextMove.weight > finalBestMove.weight){
                    finalBestMove = currentNextMove;
                    currentBestMoveIndex = i;
                }

            }

            char message[64];
            sprintf(message, "PLAY %s:%s\n",clientReachables[currentBestMoveIndex].x, clientReachables[currentBestMoveIndex].y);
            write(pipe,message, strlen(message));
        }  
    }
    return EXIT_SUCCESS;
}

void checkOrthogonalPlacing(int i, int l, int board[3][8], struct shmem* sharedMem, struct player* players){
    int x = 0;
    int y = 0;
    int opponentCounter = 0;
    int clientCounter = 0;
    for(int j = l; j<= l+2; j++){
        if(sharedMem->board[i % 8][j % 8]-1 == players[1].playerNumber){
            opponentCounter++;
            board[i % 8][j % 8] = -100;
        }
        else if(sharedMem->board[i % 8][j % 8] == 0){
            x = i % 8;
            y = j % 8;
        }
        else if(sharedMem->board[i % 8][j % 8]-1 == players[0].playerNumber){
            clientCounter++;
            board[i % 8][j % 8] = -100;
        }
    }
    if(opponentCounter - clientCounter == 2){
        board[x][y] += BLOCKMILL;
    }
    if(clientCounter - opponentCounter == 1){
        board[x][y] += ALIGNMENT;
    }
    else if(clientCounter - opponentCounter == 2){
        board[x][y] += MILL;
    }
}
int computeAbsoluteWeights(tupel clientReachables[], tupel opponentReachables[], int clientReachablesCounter, int opponentReachablesCounter){
    tupel reducedOpponentsReachables[opponentReachablesCounter];
    memcpy(reducedOpponentsReachables,opponentReachables,opponentReachablesCounter*sizeof(tupel));
    for(int i = 0; i<opponentReachablesCounter; i++){ //markieren aller reachables die auf dasselbe feld zeigen und nicht den höchsten weight haben
        int bestWeight = reducedOpponentsReachables[i].weight;
        for(int j = i+1; j<opponentReachablesCounter; j++){
            if(strcmp(reducedOpponentsReachables[i].y, reducedOpponentsReachables[j].y) == 0){
                if(reducedOpponentsReachables[j].weight > bestWeight){
                    bestWeight = reducedOpponentsReachables[j].weight;
                }
                strcpy(reducedOpponentsReachables[j].x, "rm");
                strcpy(reducedOpponentsReachables[j].y, "rm");
            }
        }
        reducedOpponentsReachables[i].weight = bestWeight;
    }
    for(int i = 0; i<opponentReachablesCounter; i++){ //addieren der weights
        if(strcmp(reducedOpponentsReachables[i].y, "rm") != 0){
            for(int j = 0; j<clientReachablesCounter; j++){
                if(strcmp(clientReachables[j].y, reducedOpponentsReachables[i].y) == 0){
                    clientReachables[j].weight += reducedOpponentsReachables[i].weight;
                }
            }
        }
    }
        // for(int i = 0; i<clientReachablesCounter; i++){ //ausgabe der finalen reachables

        // }
    return EXIT_SUCCESS;
}
int computeReachables(tupel reachables[], int board[3][8], int player, int* k){
    *k = 0;
    for(int i = 0; i<3; i++){
       for(int j = 0; j<8; j++){
            if(board[i][j]-1 == player){
                if(j % 2 == 0){
                    if(board[i][(j+7) % 8] == 0){
                        transformCoordinatesInverse(i,j,reachables[*k].x);
                        transformCoordinatesInverse(i,(j+7) % 8,reachables[*k].y);
                        if((board[i][(j+6) % 8]-1 == player) && (board[i][(j+1) % 8]-1 == player)){
                                reachables[*k].weight = MILLNEXTROUND; //chance for making a mill next round
                        }
                        switch(i){
                            case 0:
                                if(board[i+1][(j+7) % 8]-1 == player){
                                    reachables[*k].weight = 1; 
                                    if(board[i+2][(j+7) % 8]-1 == player){
                                        reachables[*k].weight = MILL; //chance of making a mill 
                                    }
                                }
                            break;
                            case 1:
                                if(board[i+1][(j+7) % 8]-1 == player){
                                    if(board[i-1][(j+7) % 8]-1 == player){
                                        reachables[*k].weight = MILL; //chance of ma*king a mill 
                                    }
                                }
                            break;
                            case 2:
                                if(board[i-1][(j+7) % 8]-1 == player){
                                    reachables[*k].weight = 1; 
                                    if(board[i-2][(j+7) % 8]-1 == player){
                                        reachables[*k].weight = MILL; //chance of ma*king a mill 
                                    }
                                }
                            break;
                        }
                        (*k)++;
                    }
                    if(board[i][(j+1) % 8] == 0){
                        transformCoordinatesInverse(i,j,reachables[*k].x);
                        transformCoordinatesInverse(i,(j+1) % 8,reachables[*k].y);
                        if((board[i][(j+2) % 8]-1 == player) && (board[i][(j+7) % 8]-1 == player)){
                                reachables[*k].weight = MILLNEXTROUND; //chance for ma*king a mill next round
                        }
                        switch(i){
                            case 0:
                                if(board[i+1][(j+1) % 8]-1 == player){
                                    reachables[*k].weight = 1; 
                                    if(board[i+2][(j+1) % 8]-1 == player){
                                        reachables[*k].weight = MILL; //chance of ma*king a mill 
                                    }
                                }
                            break;
                            case 1:
                                if(board[i+1][(j+1) % 8]-1 == player){
                                    if(board[i-1][(j+1) % 8]-1 == player){
                                        reachables[*k].weight = MILL; //chance of ma*king a mill 
                                    }
                                }
                            break;
                            case 2:
                                if(board[i-1][(j+1) % 8]-1 == player){
                                    reachables[*k].weight = 1; 
                                    if(board[i-2][(j+1) % 8]-1 == player){
                                        reachables[*k].weight = MILL; //chance of ma*king a mill 
                                    }
                                }
                            break;
                        }
                        (*k)++;
                    }
                }
                else if(j % 2 == 1){
                    if(board[i][(j+7) % 8] == 0){
                        checkRadial(i, j, player, k, board, reachables, -1);
                    }
                    if(board[i][(j+1) % 8] == 0){
                        checkRadial(i, j, player, k, board, reachables, 1);
                    }
                    switch(i){
                        case 0:
                            if(board[i+1][j] == 0){
                                checkOrthogonal(i, j, player, k, board, reachables, 1);
                            }
                        break;
                        case 1:
                            if(board[i+1][j] == 0){
                                checkOrthogonal(i, j, player, k, board, reachables, 1);
                            }
                            if(board[i-1][j] == 0){
                                checkOrthogonal(i, j, player, k, board, reachables, -1);
                            }
                        break;
                        case 2:
                            if(board[i-1][j] == 0){
                               checkOrthogonal(i, j, player, k, board, reachables, -1);
                            }
                        break;
                    }
                }
            }
        }  
    }

    return EXIT_SUCCESS;
} 

void checkOrthogonal(int i, int j, int player, int* k, int board[3][8], tupel reachables[], int direction){
    transformCoordinatesInverse(i,j,reachables[*k].x);
    transformCoordinatesInverse(i+direction,j,reachables[*k].y);
    if(board[i+direction][(j+1) % 8]-1 == player){
        reachables[*k].weight = ALIGNMENT; 
        if(board[i+direction][(j+7) % 8]-1 == player){
            reachables[*k].weight = MILL; //chance of ma*king a mill 
        }
    }
    (*k)++;
}
void checkRadial(int i, int j, int player, int* k, int board[3][8], tupel reachables[], int direction){
    if(direction == 1){
        transformCoordinatesInverse(i,j,reachables[*k].x);
        transformCoordinatesInverse(i,(j+1) % 8,reachables[*k].y);
        if(board[i][(j+2) % 8]-1 == player){
            reachables[*k].weight = ALIGNMENT;
            if(board[i][(j+3) % 8]-1 == player){
                reachables[*k].weight = MILL; //chance of making a mill
            }
            else if(board[i][(j+3) % 8]-1 == -1){
                reachables[*k].weight = ALIGNMENT; //field is empty
            }
            else{
                reachables[*k].weight = -1; //field is occupied by opponent
            } 
        }
    (*k)++;
    }
    else if(direction == -1){
        transformCoordinatesInverse(i,j,reachables[*k].x);
        transformCoordinatesInverse(i,(j+7) % 8,reachables[*k].y);
        if(board[i][(j+6) % 8]-1 == player){
            reachables[*k].weight = ALIGNMENT;
            if(board[i][(j+5) % 8]-1 == player){
                reachables[*k].weight = MILLNEXTROUND; //chance of ma*king a mill
            }
            else if(board[i][(j+5) % 8]-1 == -1){
                reachables[*k].weight = ALIGNMENT; //field is empty
            }
            else{
                reachables[*k].weight = -1; //field is occupied by opponent
            } 
        }
        (*k)++;
    }
    
}
int computeBestMove(tupel reachables[], int reachableCounter, tupel* bestMove){
    int bestWeight = -10;
    for(int i = 0; i<reachableCounter; i++){
        if(reachables[i].weight > bestWeight){
            bestWeight = reachables[i].weight;
            *bestMove = reachables[i];
        }
    }       
    return EXIT_SUCCESS;
}

void capture(struct shmem* sharedMem, struct player* players, tupel* opponentBestMove ){
    tupel opponentReachables[24]; //max reachable fields are 24
     int opponentReachableCounter;
    memset(opponentReachables, 0, sizeof(tupel)*24);

    computeReachables(opponentReachables, sharedMem->board, players[1].playerNumber, &opponentReachableCounter);
    computeBestMove(opponentReachables, opponentReachableCounter, opponentBestMove);
}