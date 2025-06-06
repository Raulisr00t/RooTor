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

BOOL is_Tor_Running(){
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
        return FALSE;
    
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(LOCAL_PROXY);

    BOOL connected;

    connected = (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    closesocket(s);

    return connected;
}

BOOL is_onion_address(const char *host) {
    return (strstr(host, ".onion") != NULL) ? TRUE : FALSE;
}

req *request(const char *dsthost, const int dstport, int *request_len) {
    BOOL is_onion = is_onion_address(dsthost);
    int extra = is_onion ? strlen(dsthost) + 1 : 0;
    
    *request_len = reqsize + extra;
    
    req *req = malloc(*request_len);
    if (!req) 
        return NULL;

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

        printf("[~] Crawling: %s\n", url);

        WSADATA wsa;
 
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            printf("[-] WSAStartup failed\n");
            free(url);
            continue;
        }

        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
 
        if (s == INVALID_SOCKET) {
            printf("[-] Socket creation failed\n");
            WSACleanup();
 
            free(url);
            continue;
        }

        struct sockaddr_in sock;
 
        sock.sin_family = AF_INET;
        sock.sin_port = htons(PORT);
        sock.sin_addr.s_addr = inet_addr(LOCAL_PROXY);

        if (connect(s, (struct sockaddr*)&sock, sizeof(sock)) != 0) {
            printf("[-] Failed to connect to proxy\n");
            closesocket(s);
            WSACleanup();

            free(url);
            continue;
        }

        printf("[+] Connected to SOCKS proxy\n");

        int req_len;
        req *r = request(url, 80, &req_len);
        if (!r) {
            printf("[-] Failed to build SOCKS request\n");
            closesocket(s);
            WSACleanup();
            
            free(url);
            continue;
        }

        send(s, (char*)r, req_len, 0);

        char buf[respsize];
        int res = recv(s, buf, respsize, 0);
        if (res <= 0) {
            printf("[-] SOCKS proxy didn't respond\n");
            closesocket(s);
            free(r);
            WSACleanup();
            
            free(url);
            continue;
        }

        resp *response = (resp*)buf;
        printf("[+] SOCKS response code: %d\n", response->cd);
        if (response->cd != 90) {
            printf("[-] SOCKS Proxy refused connection\n");
            closesocket(s);
            free(r);
            WSACleanup();
            
            free(url);
            continue;
        }

        char tmp[8192];
        snprintf(tmp, sizeof(tmp),
            "GET / HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "User-Agent: Toralizer/1.0\r\n\r\n", url);

        send(s, tmp, strlen(tmp), 0);
        memset(tmp, 0, sizeof(tmp));

        int len = recv(s, tmp, sizeof(tmp) - 1, 0);
       
        if (len <= 0) 
            printf("[-] No HTTP response received from %s\n", url);
         
        else {
            tmp[len] = '\0';
    
            printf("[RESPONSE FROM %s]\n%s\n", url, tmp);
            extract_tag_content(tmp, user_tag);
        }

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

    if (!is_Tor_Running()){
        printf("[-] TOR isn't Running [-]\n");
        return 1;
    }    
    
    const char *url = argv[1];
    
    if(!is_onion_address(url)){
        printf("[!] Please Enter Dark Web Site [!]\n");
        return 1;
    }    
    
    printf("[?] Enter tag to extract (e.g., title, h1): ");
    scanf("%63s", user_tag);

    enqueue_url(url);
    start_threads(THREADS);

    Sleep(INFINITE);
 
    return 0;
}
