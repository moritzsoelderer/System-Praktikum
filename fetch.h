#ifndef FETCH
#define FETCH

    //struct für spielzug
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

int searchAndExtract(char* haystack, char* needle, char*result, char stop);

int cutLines(char* input, char** output);

int transformCoordinates(char* input, int* l, int* r);

int transformCoordinatesInverse(int l, int r, char* output);

int printBoard(int board[3][8]);

#endif