#include "rootor.h"

#pragma comment(lib, "ws2_32.lib")  

req *request(const char *dstip, const int dstport) {
    req *req = malloc(reqsize);

    req->vn = 4;
    req->cd = 1;
    req->destination_port = htons(dstport);
    req->destination_ip = inet_addr(dstip);

    strncpy(req->user_id, "RooTorz", 8);

    return req;
}

int main(int argc, char* argv[]) {
    WSADATA wsa;
    char *host;
    int port, s, connection;

    struct sockaddr_in sock;

    req* r;
    resp* response;

    int success;

    char buf[respsize];
    char tmp[512];

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[-] WSAStartup Failed: %d\n", WSAGetLastError());
        return 1;
    }

    if (argc != 3) {
        printf("[!] USAGE: rootor.exe <HOSTNAME> <PORT>\n");
        WSACleanup();
        
        return 1;
    }

    host = argv[1];
    port = atoi(argv[2]);
    s = socket(AF_INET, SOCK_STREAM, 0);

    if (s == INVALID_SOCKET) {
        printf("[-] Socket Initialization Error\n");
        printf("[!] ERROR: %d\n", WSAGetLastError());
        WSACleanup();
       
        return 1;
    }

    sock.sin_family = AF_INET;
    sock.sin_port = htons(PORT);             
    sock.sin_addr.s_addr = inet_addr(LOCAL_PROXY);  

    connection = connect(s, (struct sockaddr*)&sock, sizeof(sock));

    if (connection != 0) {
        printf("[-] Connection Error\n");
        printf("[!] ERROR: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();

        return 1;
    }

    printf("[+] Connection Successful To PROXY [+]\n");

    r = request(host, port);

    if (send(s, (char*)r, reqsize, 0) < 0) {
        printf("[-] Error sending request\n");
        printf("[!] ERROR: %d\n", WSAGetLastError());
        free(r);
        closesocket(s);
        WSACleanup();
        
        return 1;
    }

    memset(buf, 0, respsize);

    if (recv(s, buf, respsize, 0) < 1) {
        printf("[-] Reading Buffer Error\n");
        free(r);
        closesocket(s);
        WSACleanup();
        
        return 1;
    }

    response = (resp*)buf;
    success = response->cd;

    if (success != 90) {
        printf("[-] Unable To Traverse\n");
        printf("[!] ERROR CODE: %d\n", success);
        free(r);
        closesocket(s);
        WSACleanup();
       
        return 1;
    }

    printf("[+] Successfully connected to %s:%d through the Proxy [+]\n", host, port);

    memset(tmp, 0, 512);
    snprintf(tmp,511,
    "HEAD / HTTP/1.0\r\n"
    "Host: %s\r\n"
    "\r\n" , host
    );

    send(s, tmp ,strlen(tmp), 0);

    memset(tmp, 0 ,512);

    recv(s, tmp, 511, 0);
    printf("'%s'\n", tmp);

    closesocket(s);
    free(r);

    WSACleanup();

    return 0;
}
