#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

    //struct für tupel
    typedef struct t{
        char x[3];
        char y[3];
        int weight;  
    } tupel;

    //struct für Spieler
    struct player{
        unsigned int playerNumber;
        char playerName[31];
        int ready;
        unsigned int toCapture;
        unsigned int availablePieces;
        unsigned int capturedPieces;
        unsigned int numPieces;

    };

    //SHM struct
    struct shmem{
        int flag;
        char gamename[101];
        unsigned int playerCounter;
        unsigned int ppid;
        unsigned int cpid;
        struct player* players;
        int playerShmid;
        int board[3][8];
    };


    int searchAndExtract(char* haystack, char* needle, char*result, char stop){
        char* pointer;
        //printf("NEEDLE: %s\n", needle);
        if((pointer = strstr(haystack, needle)) == NULL){
            return EXIT_FAILURE;
        }
        for(int i = 0; pointer[strlen(needle)+i] != stop; i++){
            result[i] = pointer[strlen(needle)+i];
        }
        //printf("RESULT: %s\n", result);
        return EXIT_SUCCESS;
    }

    int cutLines(char* input, char** output){
        printf("hallo ich bin in cutlines\n");
        char* occ;
        int i = 1;
        *(output) = input;
        while((occ = strchr(input, '\n')) != NULL){
            printf("%s\n",occ);
            *(output +i*sizeof(char)) = occ;
            *(occ) = '\0';
            input = occ+sizeof(char);
            i++;
        }
        return EXIT_SUCCESS;
    }

    int transformCoordinates(char* input, int* l, int* r){
        char left;
        sscanf(input,"%c%i",&left, r);

        if(left == 'A'){
            *l = 0;
        }   
        else if(left == 'B'){
            *l = 1;
        }
        else if(left == 'C'){
            *l = 2;
        }
        else{
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    int transformCoordinatesInverse(int l, int r, char* output){
        char left;

        if(l == 0){
            left = 'A';
        }   
        else if(l == 1){
            left = 'B';
        }
        else if(l == 2){
            left = 'C';
        }
        else{
            return EXIT_FAILURE;
        }
        sprintf(output, "%c%i", left, r);
        return EXIT_SUCCESS;
    }

    int printBoard(int board[3][8]){
        printf("(%i)------------(%i)------------(%i)\n", board[0][0], board[0][1], board[0][2]);
        printf(" |               |             |\n");
        printf(" |   (%i)-------(%i)-------(%i)   |\n", board[1][0], board[1][1], board[1][2]);
        printf(" |    |          |        |    |\n");
        printf(" |    |   (%i)--(%i)--(%i)   |    |\n", board[2][0], board[2][1], board[2][2]);
        printf(" |    |    |         |    |    |\n");
        printf("(%i)--(%i)--(%i)       (%i)--(%i)--(%i)\n",board[0][7], board[1][7], board[2][7], board[2][3], board[1][3], board[0][3]);
        printf(" |    |    |         |    |    |\n");
        printf(" |    |   (%i)--(%i)--(%i)   |    |\n", board[2][6], board[2][5], board[2][4]);
        printf(" |    |          |        |    |\n");
        printf(" |   (%i)-------(%i)-------(%i)   |\n", board[1][6], board[1][5], board[1][4]);
        printf(" |               |             |\n");
        printf("(%i)------------(%i)------------(%i)\n", board[0][6], board[0][5], board[0][4]);
        return EXIT_SUCCESS;
    }