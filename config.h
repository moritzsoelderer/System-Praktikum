#ifndef CLIENTCONFIG
#define CLIENTCONFIG

struct conf{
    char hostname[128];
    char gamekindname[32];
    unsigned short port;
};
int loadConf(const char* dateipfad, struct conf* config);

#endif