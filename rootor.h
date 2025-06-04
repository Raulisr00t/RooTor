#include <WinSock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LOCAL_PROXY "127.0.0.1"
#define PORT 9050
#define reqsize sizeof(struct Request)
#define respsize sizeof(struct Response)

typedef unsigned char int8;
typedef unsigned short int int16;
typedef unsigned int int32;

struct Request {
    int8 vn;
    int8 cd;
    int16 destination_port;
    int32 destination_ip;
    unsigned char user_id[8];
};

typedef struct Request req;

struct Response {
    int8 vn;
    int8 cd;
    int16 _;
    int32 __;
};

typedef struct Response resp;

req *request(const char *dstip,const int dstport);