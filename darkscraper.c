#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

#pragma comment(lib,"ws2_32.lib")

struct Memory {
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;

    mem->data = realloc(mem->data, mem->size + real_size + 1);
    if (mem->data == NULL) return 0;

    memcpy(&(mem->data[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->data[mem->size] = 0;

    return real_size;
}

int is_Tor_Running() {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server;
    int result;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 0;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(9050);

    result = connect(sock, (struct sockaddr*)&server, sizeof(server));
    closesocket(sock);
    WSACleanup();

    return result == 0;  // 0 = connection successful
}

void extract_info(const char *html, const char *tagname) {
    htmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) {
        fprintf(stderr, "Failed to parse HTML.\n");
        return;
    }

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);

    char xpath[100];
    snprintf(xpath, sizeof(xpath), "//%s", tagname); // e.g., //a or //meta

    xmlXPathObjectPtr result = xmlXPathEvalExpression((xmlChar *)xpath, ctx);
    if (result && result->nodesetval) {
        int total = result->nodesetval->nodeNr;
        printf("\nFound %d <%s> elements:\n\n", total, tagname);
        for (int i = 0; i < total; i++) {
            xmlNodePtr node = result->nodesetval->nodeTab[i];
            xmlChar *content = xmlNodeGetContent(node);
            printf("<%s>", tagname);
            if (content && xmlStrlen(content) > 0) {
                printf(" %s", content);
            }
            if (content) xmlFree(content);

            // Print all attributes
            for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
                xmlChar *value = xmlNodeListGetString(doc, attr->children, 1);
                if (value) {
                    printf(" [%s=\"%s\"]", attr->name, value);
                    xmlFree(value);
                }
            }
            printf("\n");
        }
    } 
    
    else 
        printf("No <%s> elements found.\n", tagname);
    

    xmlXPathFreeObject(result);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
}

int main(int argc, char *argv[]) {
    if (argc != 2){
        printf("[USAGE] %s <url>\n", argv[0]);
        return 1;
    }

    if (!is_Tor_Running()){
        fprintf(stderr,"[ ERROR ] Tor is not running on 127.0.0.1:9050.\nStart Tor Service!\n");
        return 1;
    }

    char tagname[50];
    printf("Enter tag name to extract (e.g., a, h1, meta, p): ");

    fgets(tagname, sizeof(tagname), stdin);
    tagname[strcspn(tagname, "\n")] = 0; 

    const char *url = argv[1];

    CURL *curl = curl_easy_init();
    struct Memory chunk = {0};

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_PROXY, "socks5h://127.0.0.1:9050"); 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) 
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         
        else 
            extract_info(chunk.data, tagname);

        free(chunk.data);
        curl_easy_cleanup(curl);
    }

    return 0;
}
