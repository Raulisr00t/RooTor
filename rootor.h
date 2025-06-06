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
#define MAX_QUEUE 1024
#define THREADS 4

char *url_queue[MAX_QUEUE];
int queue_start = 0, queue_end = 0;
CRITICAL_SECTION queue_lock;

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

req *request(const char *dstip,const int dstport, int *request_length);
void extract_tags(const char* html,const char* tag);
char *dequeue_url();
DWORD WINAPI crawl_worker(LPVOID param);
void start_threads(int t_count);
void enqueue_url(const char* url);
BOOL is_onion_address(const char *host);
char user_tag[64];
