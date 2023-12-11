#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


//configuration struct
struct conf{
    char hostname[128];
    char gamekindname[32];
    unsigned short port;
};

//load configuration into struct
int loadConf(const char* dateipfad, struct conf* config){
    FILE* datei;
    if((datei = fopen(dateipfad, "r")) == NULL){
        perror("Conf-Datei konnte nicht geoeffnet werden: ");
        return EXIT_FAILURE;
    }
    char tag[64];
    char value[64];
    while(fscanf(datei,"%s = %s\n", tag, value) != EOF){
        if(strcmp(tag, "hostname") == 0){
            strcpy(config->hostname, value);
        }
        else if(strcmp(tag, "port") == 0){
            sscanf(value, "%hu", &config->port);
        }
        else if(strcmp(tag, "gamekindname") == 0){
            strncpy(config->gamekindname, value, strlen(value));
        }
    }
    
    if((fclose(datei)) != 0){
        perror("Filedescriptor konnte nicht geschlossen werden: ");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}