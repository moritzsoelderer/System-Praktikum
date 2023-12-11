#ifndef PERFORMCONNECTION
#define PERFORMCONNECTION

#include "fetch.h"

int performConnection(int sock, char* gameId,char* gkn, int buffer, struct shmem* sharedMem, int pipe);

#endif