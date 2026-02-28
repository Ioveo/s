#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>

__declspec(dllexport) int expand_ip_range(const char *line, char ***out, size_t *count) {
    if (!line || !out || !count) return -1;
    *out = NULL; *count = 0;

    char buf[1024];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    if (!*s || *s == '#') return -1;

    char ip_part[256];
    char suffix[256] = "";
    
    char *colon = NULL;
    char *slash = strchr(s, '/');
    if (slash) {
        colon = strchr(slash, ':');
    } else {
        char *dash = strchr(s, '-');
        if (dash) colon = strchr(dash, ':');
        else colon = strchr(s, ':');
    }

    if (colon) {
        strncpy(suffix, colon, sizeof(suffix) - 1);
        *colon = '\0';
    }
    strncpy(ip_part, s, sizeof(ip_part) - 1);
    ip_part[sizeof(ip_part) - 1] = '\0';

    uint32_t start_ip = 0, end_ip = 0;

#define IP_TO_U32(str, val) do { \
    struct in_addr _a; \
    if (inet_pton(AF_INET, (str), &_a) != 1) return -1; \
    (val) = ntohl(_a.s_addr); \
} while(0)

    slash = strchr(ip_part, '/');
    if (slash) {
        *slash = '\0';
        int prefix = atoi(slash + 1);
        if (prefix < 0 || prefix > 32) return -1;
        IP_TO_U32(ip_part, start_ip);
        uint32_t mask = prefix == 0 ? 0 : (~0u << (32 - prefix));
        start_ip = (start_ip & mask) + 1;
        end_ip   = (start_ip - 1) | (~mask & 0xFFFFFFFE);
        if (prefix >= 31) { start_ip = (start_ip & mask); end_ip = start_ip; }
        goto expand;
    }

    char *dash = strchr(ip_part, '-');
    if (dash) {
        *dash = '\0';
        const char *rhs = dash + 1;
        IP_TO_U32(ip_part, start_ip);
        if (strchr(rhs, '.')) {
            IP_TO_U32(rhs, end_ip);
        } else {
            int last = atoi(rhs);
            if (last < 0 || last > 255) return -1;
            end_ip = (start_ip & 0xFFFFFF00u) | (uint32_t)last;
        }
        if (end_ip < start_ip) { uint32_t t = start_ip; start_ip = end_ip; end_ip = t; }
        goto expand;
    }

    if (inet_pton(AF_INET, ip_part, &start_ip) != 1) {
        return -1;
    }
    start_ip = ntohl(start_ip);
    end_ip = start_ip;

expand:;
    size_t n = (size_t)(end_ip - start_ip + 1);
    if (n == 0 || n > 65536) return -1;

    char **arr = (char **)malloc(n * sizeof(char *));
    if (!arr) return -1;

    for (size_t i = 0; i < n; i++) {
        uint32_t ip = start_ip + (uint32_t)i;
        struct in_addr a;
        a.s_addr = htonl(ip);
        char tmp[INET_ADDRSTRLEN + 256]; 
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &a, ip_str, sizeof(ip_str));
        
        snprintf(tmp, sizeof(tmp), "%s%s", ip_str, suffix);
        arr[i] = strdup(tmp);
        if (!arr[i]) {
            for (size_t j = 0; j < i; j++) free(arr[j]);
            free(arr);
            return -1;
        }
    }
    *out = arr;
    *count = n;
    return 0;
#undef IP_TO_U32
}
