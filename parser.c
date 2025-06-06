// For scraping dark web site's tag

#include "rootor.h"

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
