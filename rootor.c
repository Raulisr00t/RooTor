#include "rootor.h"

#pragma comment(lib, "ws2_32.lib")  

void enqueue_url(const char *url) {
    printf("[DEBUG] Entered enqueue_url() with url: %s\n", url ? url : "NULL");

    fflush(stdout);

    EnterCriticalSection(&queue_lock);

    if ((queue_end + 1) % MAX_QUEUE != queue_start) {
        url_queue[queue_end] = _strdup(url);
        printf("[DEBUG] Enqueued URL: %s (at position %d)\n", url, queue_end);
        
        queue_end = (queue_end + 1) % MAX_QUEUE;
    } 
    
    else 
        printf("[WARN] Queue is full, cannot enqueue URL: %s\n", url);
    
    fflush(stdout);
    
    LeaveCriticalSection(&queue_lock);
}

char *dequeue_url() {
    char *url = NULL;

    EnterCriticalSection(&queue_lock);
    if (queue_start != queue_end) {
        url = url_queue[queue_start];
        printf("[DEBUG] Dequeued URL: %s (from position %d)\n", url, queue_start);
        queue_start = (queue_start + 1) % MAX_QUEUE;
    } 
    
    else 
        printf("[DEBUG] Queue is empty, nothing to dequeue\n");
    
    fflush(stdout);
    LeaveCriticalSection(&queue_lock);

    return url;
}

BOOL is_Tor_Running(){
    printf("[DEBUG] Creating socket...\n");
    fflush(stdout);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);

    if (s == INVALID_SOCKET) {
        printf("[-] Socket creation failed, error: %d\n", WSAGetLastError());
        fflush(stdout);
        return FALSE;
    }
    
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);       
    addr.sin_addr.s_addr = inet_addr(LOCAL_PROXY); 

    printf("[DEBUG] Trying to connect to proxy %s:%d\n", LOCAL_PROXY, PORT);
    fflush(stdout);

    BOOL connected = (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);
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

int status_code(const char* response){
    int code = 0;

    sscanf(response,"HTTP/%*s %d",&code);
    return code;
}

char* extract_location(const char *response) {
    const char *loc = strstr(response, "\nLocation:");
   
    if (!loc) loc = strstr(response, "\nlocation:");
    if (!loc) return NULL;
   
    loc += strlen("\nLocation:");
   
    while (*loc == ' ') loc++; 

    const char *end = strchr(loc, '\r');
    if (!end) end = strchr(loc, '\n');
    if (!end) return NULL;

    size_t len = end - loc;
    char *location = malloc(len + 1);

    if (!location) return NULL;
    strncpy(location, loc, len);

    location[len] = '\0';

    return location;
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
    printf("[DEBUG] Worker thread started\n");
    fflush(stdout);

    while (1) {
        char *url = dequeue_url();

        if (url == NULL) {
            printf("[DEBUG] Queue is empty, nothing to dequeue\n");
            Sleep(500); 
            continue;   
        }

        printf("[~] Crawling: %s\n", url);
        fflush(stdout);

        WSADATA wsa;

        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            printf("[-] WSAStartup failed. Error: %d\n", WSAGetLastError());
            fflush(stdout);
   
            free(url);
            continue;
        }

        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
   
        if (s == INVALID_SOCKET) {
            printf("[-] Socket creation failed. Error: %d\n", WSAGetLastError());
            fflush(stdout);
   
            WSACleanup();
            free(url);
            continue;
        }

        struct sockaddr_in sock;
   
        sock.sin_family = AF_INET;
        sock.sin_port = htons(PORT);
        sock.sin_addr.s_addr = inet_addr(LOCAL_PROXY);

        if (connect(s, (struct sockaddr*)&sock, sizeof(sock)) != 0) {
            printf("[-] Failed to connect to proxy. Error: %d\n", WSAGetLastError());
            fflush(stdout);
            closesocket(s);
   
            WSACleanup();
   
            free(url);
            continue;
        }

        printf("[+] Connected to SOCKS proxy\n");
        fflush(stdout);

        int req_len;
        req *r = request(url, 80, &req_len);
   
        if (!r) {
            printf("[-] Failed to build SOCKS request. Error: %lu\n", GetLastError());
            fflush(stdout);
            closesocket(s);
            WSACleanup();
   
            free(url);
            continue;
        }

        if (send(s, (char*)r, req_len, 0) == SOCKET_ERROR) {
            printf("[-] Failed to send SOCKS request. Error: %d\n", WSAGetLastError());
            fflush(stdout);
            closesocket(s);
            free(r);
            WSACleanup();

            free(url);
            continue;
        }

        char buf[respsize];
        int res = recv(s, buf, respsize, 0);
        if (res <= 0) {
            printf("[-] SOCKS proxy didn't respond. recv() error: %d\n", WSAGetLastError());
            fflush(stdout);
            closesocket(s);
            free(r);
            WSACleanup();
            free(url);
            continue;
        }

        resp *response = (resp*)buf;
        printf("[+] SOCKS response code: %d\n", response->cd);
        fflush(stdout);

        if (response->cd != 90) {
            printf("[-] SOCKS Proxy refused connection\n");
            fflush(stdout);
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

        if (send(s, tmp, strlen(tmp), 0) == SOCKET_ERROR) {
            printf("[-] Failed to send HTTP GET request. Error: %d\n", WSAGetLastError());
            fflush(stdout);
            closesocket(s);
            free(r);
            WSACleanup();
            free(url);
            continue;
        }

        memset(tmp, 0, sizeof(tmp));
        int len = recv(s, tmp, sizeof(tmp) - 1, 0);
        
        if (len <= 0) {
            printf("[-] No HTTP response received from %s. recv() error: %d\n", url, WSAGetLastError());
            fflush(stdout);
        } 
        
        else {
            tmp[len] = '\0';
            printf("[RESPONSE FROM %s]\n%s\n", url, tmp);
        
            fflush(stdout);

            int status_code = parse_status_code(tmp);
        
            if (status_code >= 300 && status_code < 400) {
                char *location = extract_location(tmp);
        
                if (location) {
                    printf("[~] Redirect detected: %s\n", location);
                    enqueue_url(location);
                    free(location);
                }
                closesocket(s);
                free(r);
        
                WSACleanup();
                free(url);
        
                continue;  // Don't extract tags from redirect pages
            }

        extract_tag_content(tmp, user_tag);
        }

        closesocket(s);
        free(r);
 
        WSACleanup();
        free(url);
    }

    return 0;
}
// DWORD WINAPI crawl_worker(LPVOID param) {
//     while (1) {
//         char *url = dequeue_url();
 
//         if (!url) {
//             Sleep(100);
//             continue;
//         }

//         printf("[~] Crawling: %s\n", url);

//         WSADATA wsa;
 
//         if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
//             printf("[-] WSAStartup failed\n");
//             free(url);
//             continue;
//         }

//         SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
 
//         if (s == INVALID_SOCKET) {
//             printf("[-] Socket creation failed\n");
//             WSACleanup();
 
//             free(url);
//             continue;
//         }

//         struct sockaddr_in sock;
 
//         sock.sin_family = AF_INET;
//         sock.sin_port = htons(PORT);
//         sock.sin_addr.s_addr = inet_addr(LOCAL_PROXY);

//         if (connect(s, (struct sockaddr*)&sock, sizeof(sock)) != 0) {
//             printf("[-] Failed to connect to proxy\n");
//             closesocket(s);
//             WSACleanup();

//             free(url);
//             continue;
//         }

//         printf("[+] Connected to SOCKS proxy\n");

//         int req_len;
//         req *r = request(url, 80, &req_len);
//         if (!r) {
//             printf("[-] Failed to build SOCKS request\n");
//             closesocket(s);
//             WSACleanup();
            
//             free(url);
//             continue;
//         }

//         send(s, (char*)r, req_len, 0);

//         char buf[respsize];
//         int res = recv(s, buf, respsize, 0);
//         if (res <= 0) {
//             printf("[-] SOCKS proxy didn't respond\n");
//             closesocket(s);
//             free(r);
//             WSACleanup();
            
//             free(url);
//             continue;
//         }

//         resp *response = (resp*)buf;
//         printf("[+] SOCKS response code: %d\n", response->cd);
//         if (response->cd != 90) {
//             printf("[-] SOCKS Proxy refused connection\n");
//             closesocket(s);
//             free(r);
//             WSACleanup();
            
//             free(url);
//             continue;
//         }

//         char tmp[8192];
//         snprintf(tmp, sizeof(tmp),
//             "GET / HTTP/1.1\r\n"
//             "Host: %s\r\n"
//             "Connection: close\r\n"
//             "User-Agent: Toralizer/1.0\r\n\r\n", url);

//         send(s, tmp, strlen(tmp), 0);
//         memset(tmp, 0, sizeof(tmp));

//         int len = recv(s, tmp, sizeof(tmp) - 1, 0);
       
//         if (len <= 0) 
//             printf("[-] No HTTP response received from %s\n", url);
         
//         else {
//             tmp[len] = '\0';
    
//             printf("[RESPONSE FROM %s]\n%s\n", url, tmp);
//             extract_tag_content(tmp, user_tag);
//         }

//         closesocket(s);
//         free(r);
        
//         WSACleanup();
//         free(url);
//     }

//     return 0;
// }

void start_threads(int thread_count) {
    printf("[DEBUG] start_threads() called with %d threads\n", thread_count);
    fflush(stdout);

    InitializeCriticalSection(&queue_lock);

    for (int i = 0; i < thread_count; ++i) {
        HANDLE h = CreateThread(NULL, 0, crawl_worker, NULL, 0, NULL);

        if (h == NULL) {
            printf("[-] Failed to create thread %d, error: %lu\n", i, GetLastError());
        } 
        
        else
            printf("[DEBUG] Thread %d started successfully.\n", i);
        
        fflush(stdout);
    }
}

int main(int argc, char* argv[]) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    InitializeCriticalSection(&queue_lock);  

    if (argc != 2) {
        printf("[!] Usage: rootor.exe <onion-hostname>\n");
        return 1;
    }

    if (!is_Tor_Running()){
        printf("[-] TOR isn't Running [-]\n");
        WSACleanup();
        return 1;
    }    
    
    const char *url = argv[1];
    
    if(!is_onion_address(url)){
        printf("[!] Please Enter Dark Web Site [!]\n");
        WSACleanup();
        return 1;
    }    
    
    printf("[?] Enter tag to extract (e.g., title, h1): ");
    scanf("%63s", user_tag);

    enqueue_url(url);
    start_threads(THREADS);

    Sleep(INFINITE);
 
    return 0;
}
