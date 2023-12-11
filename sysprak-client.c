#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/shm.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "performConnection.h"
#include "config.h"
#include "fetch.h"
#include "think.h"
#define BUF 256

int connectToServer(int sock, struct conf* configStruct, struct sockaddr_in* socketAdress);
void signalhandler(int signal);

//global variables also used by signal handler
struct shmem *sharedMem;
int pipefd[2];

int main(int argc, char** argv){

    //Variablen und Structs
    char* gameId;
    char* playerNumber;
    char* configFile;
    struct conf configStruct; //muss später gefreed werden!
    memset(&configStruct, 0, sizeof(struct conf));
    int sock;

    struct sockaddr_in socketAdress;
    memset(&socketAdress,0,sizeof(socketAdress)); //Alle bytes von der Socket-Adresse auf 0 setzen (damit keine falschen Werte bleiben)

    //checken ob genuegend Kommandozeilenparameter eingegeben wurden
    char option;
    int optionCounter = 0;
    while((option = getopt(argc, argv, "g:c:p:")) != -1){
        switch(option){
            case 'g':
                gameId = optarg;
                optionCounter++;
            break;
            case 'c':
                configFile = optarg;
                optionCounter++;
            break;
            case 'p':
                playerNumber = optarg;
                optionCounter++;
            break;
        }
        printf("%i\n", optionCounter);
    }

    if(optionCounter < 3){
        puts("Es wurden zu wenig Kommandozeilenparameter übergeben.");
        return EXIT_FAILURE;
    }
        
    puts(playerNumber); //damit kein error gibt;

    //load config
    loadConf(configFile, &configStruct);
    
    //Socket erstellen
    if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
        perror("Socket-Erstellen fehlgeschlagen");
        return EXIT_FAILURE;
    }
    //eigene Methode für getaddrinfo + connect
    if(connectToServer(sock, &configStruct, &socketAdress) != 0){
        return EXIT_FAILURE;
    }

    //Pipe erstellen
    if(pipe(pipefd) < 0){
        perror("Pipe-Erstellung fehlgeschlagen");
        return EXIT_FAILURE;
    }
    //SHM erstellen
    int shmid;
    if((shmid = shmget(IPC_PRIVATE, sizeof(struct shmem),IPC_CREAT | 0600)) == -1){
        perror("SHM-Erstellung fehlgeschlagen");
        return EXIT_FAILURE;
    }
    //Prozess an SHM anbinden (wird vererbt!)
    if((sharedMem = shmat(shmid,NULL, 0)) == (void*)-1){
        perror("SHM-Anbindung fehlgeschlagen");
        return EXIT_FAILURE;
    }    
    //Kindprozess erstellen
    int pid;
    if((pid = fork()) < 0){
        perror("Prozessteilung fehlgeschlagen");
        return EXIT_FAILURE;
    }

    //thinker - Elternprozess
    else if(pid > 0){
        close(pipefd[0]); //closing reading end of pipe
        signal(SIGUSR1, signalhandler); //signal abfangen (vielleicht auf sigaction umruesten da plattformunabhängiger)
       
        //auf Kindprozess warten
        int stat = 0;
        if(wait(&stat) == -1){
            perror("Probleme bei der Prozesskommunikation.");
        }

        //Socket schließen
        printf("Die Verbindung wird geschlossen...\n");
        close(sock); //socket schließen
        shmdt(sharedMem);
        shmctl(shmid,IPC_RMID, NULL); //SHM schließen
    }
    //connector - Kindprozess
    else if(pid  == 0){
        close(pipefd[1]); //closing writing end of pipe
        puts(configStruct.gamekindname);
        printf("%li\n", strlen(configStruct.gamekindname));
        char* gkn = configStruct.gamekindname;
        if(performConnection(sock,gameId,gkn,BUF, sharedMem, pipefd[0]) != 0){
            printf("Es ist ein Problem aufgetreten, der Server hat die Verbindung getrennt.\n");
            shmdt(sharedMem);
            return EXIT_FAILURE;
        }
        shmdt(sharedMem); //dettaching SHM-Segment
    }
}

int connectToServer(int sock, struct conf* configStruct, struct sockaddr_in* socketAdress){
    //IP-Adresse holen
    struct addrinfo *res = NULL;
    struct addrinfo hints;
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    char port[14];
    sprintf(port, "%i",configStruct->port);
    if((getaddrinfo(configStruct->hostname, port,&hints, &res)) != 0){
        printf("%s\n", "Adressinfo konnte nicht geladen werden");
        return EXIT_FAILURE;
    }
    *socketAdress = *((struct sockaddr_in*) res->ai_addr); //zuweisen der Adresse aus res
    
    freeaddrinfo(res); //freigeben der addrinfo von res
 
    //mit Server verbinden
    if((connect(sock, (struct sockaddr*)socketAdress, sizeof(*socketAdress))) == -1){
        perror("Verbindung fehlgeschlagen - ");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void signalhandler(int signal){
    puts("im signalhandler");
    think(sharedMem,pipefd[1], BUF);
} 