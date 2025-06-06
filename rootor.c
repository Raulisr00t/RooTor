#include "rootor.h"

#pragma comment(lib, "ws2_32.lib")  

void enqueue_url(const char *url) {
    EnterCriticalSection(&queue_lock);
    if ((queue_end + 1) % MAX_QUEUE != queue_start) {
        url_queue[queue_end] = _strdup(url);
        queue_end = (queue_end + 1) % MAX_QUEUE;
    }
    LeaveCriticalSection(&queue_lock);
}

char *dequeue_url() {
    char *url = NULL;
    EnterCriticalSection(&queue_lock);
    if (queue_start != queue_end) {
        url = url_queue[queue_start];
        queue_start = (queue_start + 1) % MAX_QUEUE;
    }
    LeaveCriticalSection(&queue_lock);
    return url;
}

BOOL is_onion_address(const char *host) {
    return (strstr(host, ".onion") != NULL) ? TRUE : FALSE;
}

req *request(const char *dsthost, const int dstport, int *request_len) {
    BOOL is_onion = is_onion_address(dsthost);
    int extra = is_onion ? strlen(dsthost) + 1 : 0;
    *request_len = reqsize + extra;
    req *req = malloc(*request_len);
    if (!req) return NULL;

    req->vn = 4;
    req->cd = 1;
    req->destination_port = htons(dstport);
    req->destination_ip = is_onion ? htonl(0x00000001) : inet_addr(dsthost);
    strncpy((char*)req->user_id, "RooTorz", 8);

    if (is_onion) {
        char *domain = (char*)req + reqsize;
        strcpy(domain, dsthost);
    }

    return req;
}

void extract_tag_content(const char* html, const char* tag) {
    size_t tag_len = strlen(tag);
    char open_tag[64], close_tag[64];

    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *p = html;
    
    while ((p = strstr(p, open_tag)) != NULL) {
        p += strlen(open_tag);

        const char *end = strstr(p, close_tag);
        if (!end) break;

        size_t content_len = end - p;
    
        if (content_len < 512) {
            char content[512];
            
            strncpy(content, p, content_len);
            content[content_len] = '\0';
    
            printf("[TAG -> %s] %s\n", tag, content);
        }
        p = end + strlen(close_tag);
    }
}

DWORD WINAPI crawl_worker(LPVOID param) {
    while (1) {
        char *url = dequeue_url();
        if (!url) {
            Sleep(100);
            continue;
        }

        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sock;

        sock.sin_family = AF_INET;
        sock.sin_port = htons(PORT);
        sock.sin_addr.s_addr = inet_addr(LOCAL_PROXY);

        if (connect(s, (struct sockaddr*)&sock, sizeof(sock)) != 0) {
            printf("[-] Failed to connect to proxy\n");
            closesocket(s);
            continue;
        }

        int req_len;
        req *r = request(url, 80, &req_len);
        
        send(s, (char*)r, req_len, 0);
        char buf[respsize];
        
        recv(s, buf, respsize, 0);

        resp *response = (resp*)buf;
        
        if (response->cd != 90) {
            printf("[-] SOCKS Proxy refused connection\n");
            closesocket(s);
            free(r);
            continue;
        }

        char tmp[2048];
        snprintf(tmp, sizeof(tmp),
            "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", url);

        send(s, tmp, strlen(tmp), 0);
        memset(tmp, 0, sizeof(tmp));
        recv(s, tmp, sizeof(tmp)-1, 0);

        printf("[RESPONSE FROM %s]\n%s\n", url, tmp);
        //extract_links(tmp);
       
        extract_tag_content(tmp, user_tag);

        closesocket(s);
        free(r);
        WSACleanup();
        free(url);
    }
    return 0;
}

void start_threads(int thread_count) {
    InitializeCriticalSection(&queue_lock);
    for (int i = 0; i < thread_count; ++i) {
        CreateThread(NULL, 0, crawl_worker, NULL, 0, NULL);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("[!] Usage: rootor.exe <onion-hostname>\n");
        return 1;
    }

    printf("[?] Enter tag to extract (e.g., title, h1): ");
    scanf("%63s", user_tag);

    const char *url = argv[1];

    enqueue_url(url);
    start_threads(THREADS);

    Sleep(INFINITE);
 
    return 0;
}
