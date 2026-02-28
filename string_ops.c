#include "saia.h"

// 简单的正则匹配（使用strstr作为简化实现）
int regex_match(const char *pattern, const char *str) {
    if (!pattern || !str) return 0;
    
    // 简化实现：如果模式不包含特殊字符，使用strstr
    if (strchr(pattern, '*') == NULL && 
        strchr(pattern, '?') == NULL && 
        strchr(pattern, '[') == NULL) {
        return strstr(str, pattern) != NULL;
    }
    
    // 通配符简单实现
    // 这里只是一个基础实现
    while (*pattern && *str) {
        if (*pattern == '*') {
            // *匹配任意序列
            pattern++;
            if (*pattern == '\0') return 1;
            while (*str) {
                if (regex_match(pattern, str++)) {
                    return 1;
                }
            }
            return 0;
        } else if (*pattern == '?') {
            // ?匹配任意单个字符
            pattern++;
            str++;
        } else if (*pattern == *str) {
            pattern++;
            str++;
        } else {
            return 0;
        }
    }
    
    if (*pattern == '\0' && *str == '\0') return 1;
    if (*pattern == '*' && *(pattern + 1) == '\0') return 1;
    
    return 0;
}

// 简化的正则提取
int regex_extract(const char *pattern, const char *str, char **matches, size_t *count) {
    if (!pattern || !str || !matches || !count) {
        return -1;
    }
    
    *matches = NULL;
    *count = 0;
    
    // 简化实现：提取模式中的变量
    // 这里只是一个占位符实现
    (void)pattern;
    (void)str;
    
    return -1;
}

// 解析凭据行
int parse_credentials(const char *line, credential_t *cred) {
    if (!line || !cred) return -1;
    
    memset(cred, 0, sizeof(credential_t));
    
    // 格式: username:password 或 password
    char *colon = strchr(line, ':');
    if (colon) {
        size_t user_len = colon - line;
        if (user_len > 0 && user_len < sizeof(cred->username)) {
            memcpy(cred->username, line, user_len);
            cred->username[user_len] = '\0';
        }
        
        const char *pass = colon + 1;
        size_t pass_len = strlen(pass);
        if (pass_len > 0 && pass_len < sizeof(cred->password)) {
            strncpy(cred->password, pass, sizeof(cred->password) - 1);
        }
    } else {
        // 只有密码，使用默认用户名
        strncpy(cred->password, line, sizeof(cred->password) - 1);
        strcpy(cred->username, "admin");
    }
    
    return 0;
}

// 解析IP:端口:用户名:密码或类似格式
int parse_ip_port_user_pass(const char *line, ip_port_t *addr, credential_t *cred) {
    if (!line || !addr || !cred) return -1;
    
    memset(addr, 0, sizeof(ip_port_t));
    memset(cred, 0, sizeof(credential_t));
    
    // 格式: IP:PORT:USERNAME:PASSWORD 或类似
    char *copy = strdup(line);
    if (!copy) return -1;
    
    char *parts[4] = {0};
    int part_count = 0;
    char *p = copy;
    
    for (int i = 0; i < 4 && p; i++) {
        char *next = strchr(p, ':');
        if (next) {
            *next = '\0';
            parts[i] = p;
            p = next + 1;
            part_count++;
        } else {
            parts[i] = p;
            p = NULL;
            part_count++;
        }
    }
    
    // 解析IP和端口
    if (parts[0]) {
        char *port_sep = strchr(parts[0], ':');
        if (port_sep) {
            *port_sep = '\0';
            ip_parse(parts[0], &addr->ip);
            addr->port = (uint16_t)atoi(port_sep + 1);
        } else {
            ip_parse(parts[0], &addr->ip);
            if (parts[1]) {
                addr->port = (uint16_t)atoi(parts[1]);
            }
        }
    }
    
    // 解析用户名和密码
    int user_idx = 2;
    int pass_idx = 3;
    
    // 如果端口在第二部分
    if (parts[1] && strchr(parts[1], ':') == NULL && atoi(parts[1]) > 0) {
        user_idx = 2;
        pass_idx = 3;
    }
    
    if (user_idx < part_count && parts[user_idx]) {
        strncpy(cred->username, parts[user_idx], sizeof(cred->username) - 1);
    } else {
        strcpy(cred->username, "admin");
    }
    
    if (pass_idx < part_count && parts[pass_idx]) {
        strncpy(cred->password, parts[pass_idx], sizeof(cred->password) - 1);
    }
    
    free(copy);
    return 0;
}

// 提取国家和ASN信息
int extract_country_asn(const char *line, char *country, char *asn) {
    if (!line || !country || !asn) return -1;
    
    country[0] = '\0';
    asn[0] = '\0';
    
    // 查找国家代码 (2字母代码)
    const char *p = strstr(line, "|");
    while (p && *(p + 1)) {
        p++;
        
        // 跳过空格
        while (*p && (*p == ' ' || *p == '\t')) p++;
        
        // 检查是否是2字母国家代码
        if (isupper(*p) && isupper(*(p + 1)) && 
            (isspace(*(p + 2)) || *(p + 2) == '|' || *(p + 2) == '\0')) {
            country[0] = *p;
            country[1] = *(p + 1);
            country[2] = '\0';
            
            // 检查下一个是否是ASN
            const char *asn_start = p + 2;
            while (*asn_start && isspace(*asn_start)) asn_start++;
            
            if (strncasecmp(asn_start, "AS", 2) == 0) {
                const char *asn_end = asn_start + 2;
                while (*asn_end && isdigit(*asn_end)) asn_end++;
                size_t asn_len = asn_end - asn_start;
                if (asn_len < 32) {
                    strncpy(asn, asn_start, asn_len);
                    asn[asn_len] = '\0';
                }
            }
            
            return 0;
        }
        
        // 移动到下一个"|"
        p = strchr(p, '|');
    }
    
    // 尝试直接查找ASN
    const char *asn_pos = strstr(line, "AS");
    if (asn_pos) {
        const char *asn_end = asn_pos + 2;
        while (*asn_end && isdigit(*asn_end)) asn_end++;
        size_t asn_len = asn_end - asn_pos;
        if (asn_len >= 3 && asn_len < 32) {
            strncpy(asn, asn_pos, asn_len);
            asn[asn_len] = '\0';
        }
    }
    
    return 0;
}

// 提取RTT毫秒数
int extract_rtt_ms(const char *line) {
    if (!line) return -1;
    
    // 查找 "rtt:" 或 "RTT:"
    const char *patterns[] = {"rtt:", "RTT:", "rtt=", "RTT=", "rtt ", "RTT "};
    
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
        const char *pos = strstr(line, patterns[i]);
        if (pos) {
            const char *val_start = pos + strlen(patterns[i]);
            while (*val_start && isspace(*val_start)) val_start++;
            
            char *endptr;
            long val = strtol(val_start, &endptr, 10);
            if (val > 0 && val < 60000) {  // 合理范围
                return (int)val;
            }
        }
    }
    
    return -1;
}

// ==================== IP 段/范围展开 ====================

/*
 * expand_ip_range: 将一行文本中的 IP 描述展开为独立 IP 字符串数组。
 * 支持格式:
 *   单 IP      1.2.3.4
 *   IP:PORT    1.2.3.4:8080   (仅提取 IP)
 *   CIDR       1.2.3.0/24
 *   范围(全)   1.2.3.1-1.2.3.254
 *   范围(末端) 1.2.3.1-254
 *
 * 参数: line   输入字符串
 *       out    *out 指向 malloc 分配的 char* 数组
 *       count  展开后的 IP 数量
 * 返回: 0=成功 -1=失败/无效
 * 调用方要 free(*out)[i] 和 free(*out) 本身
 */
int expand_ip_range(const char *line, char ***out, size_t *count) {
    if (!line || !out || !count) return -1;
    *out = NULL; *count = 0;

    /* 去掉前导/尾部空白 */
    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    /* trim */
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    if (!*s || *s == '#') return -1;

    /* 去掉 IP:PORT 末尾端口（保留 IP 部分） */
    char ip_part[128];
    strncpy(ip_part, s, sizeof(ip_part) - 1);
    ip_part[sizeof(ip_part) - 1] = '\0';

    /* 检测是否有冒号（IPv4 含端口） — 只处理 IPv4 */
    char *colon = strchr(ip_part, ':');
    if (colon) *colon = '\0';  /* 截掉端口 */

    /* 工具：将 "a.b.c.d" 转为 32 位网络序整数 */
#define IP_TO_U32(str, val) do { \
    struct in_addr _a; \
    if (inet_pton(AF_INET, (str), &_a) != 1) return -1; \
    (val) = ntohl(_a.s_addr); \
} while(0)

    uint32_t start_ip = 0, end_ip = 0;

    /* 情况 1: CIDR  a.b.c.d/prefix */
    char *slash = strchr(ip_part, '/');
    if (slash) {
        *slash = '\0';
        int prefix = atoi(slash + 1);
        if (prefix < 0 || prefix > 32) return -1;
        IP_TO_U32(ip_part, start_ip);
        uint32_t mask = prefix == 0 ? 0 : (~0u << (32 - prefix));
        start_ip = (start_ip & mask) + 1;          /* 跳过网络地址 */
        end_ip   = (start_ip - 1) | (~mask & 0xFFFFFFFE); /* 跳过广播地址 */
        if (prefix >= 31) { start_ip = (start_ip & mask); end_ip = start_ip; }
        goto expand;
    }

    /* 情况 2: 范围  a.b.c.x-a.b.c.y  或  a.b.c.x-y */
    char *dash = strchr(ip_part, '-');
    if (dash) {
        *dash = '\0';
        const char *rhs = dash + 1;
        IP_TO_U32(ip_part, start_ip);
        /* 判断右侧是完整 IP 还是末尾八位 */
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

    /* 情况 3: 单 IP */
    IP_TO_U32(ip_part, start_ip);
    end_ip = start_ip;

expand:;
    size_t n = (size_t)(end_ip - start_ip + 1);
    /* 安全上限：单次展开不超过 65536 个 IP（/16 以下用 CIDR 合并） */
    if (n == 0 || n > 65536) return -1;

    char **arr = (char **)malloc(n * sizeof(char *));
    if (!arr) return -1;

    for (size_t i = 0; i < n; i++) {
        uint32_t ip = start_ip + (uint32_t)i;
        struct in_addr a;
        a.s_addr = htonl(ip);
        char tmp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &a, tmp, sizeof(tmp));
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

/*
 * expand_nodes_list: 读取节点文件中的每一行，展开 IP 段后合并为一个大数组。
 * 调用方对 (*expanded)[i] 和 (*expanded) 本身负责 free。
 */
int expand_nodes_list(char **raw_lines, size_t raw_count,
                      char ***expanded, size_t *exp_count) {
    if (!raw_lines || !expanded || !exp_count) return -1;
    *expanded = NULL; *exp_count = 0;

    /* 预估容量 */
    size_t cap = raw_count * 4 + 16;
    char **arr = (char **)malloc(cap * sizeof(char *));
    if (!arr) return -1;
    size_t total = 0;

    for (size_t li = 0; li < raw_count; li++) {
        const char *line = raw_lines[li];
        if (!line || !*line || *line == '#') continue;

        char **sub = NULL;
        size_t sub_n = 0;
        if (expand_ip_range(line, &sub, &sub_n) == 0 && sub && sub_n > 0) {
            /* 扩容 */
            if (total + sub_n >= cap) {
                cap = (total + sub_n) * 2 + 16;
                char **tmp = (char **)realloc(arr, cap * sizeof(char *));
                if (!tmp) {
                    for (size_t j = 0; j < sub_n; j++) free(sub[j]);
                    free(sub);
                    continue;
                }
                arr = tmp;
            }
            for (size_t si = 0; si < sub_n; si++) arr[total++] = sub[si];
            free(sub);
        } else {
            /* 如果解析失败（如带端口格式）直接保留原始行 */
            if (total + 1 >= cap) {
                cap = cap * 2 + 16;
                char **tmp = (char **)realloc(arr, cap * sizeof(char *));
                if (!tmp) continue;
                arr = tmp;
            }
            arr[total++] = strdup(line);
        }
    }

    *expanded = arr;
    *exp_count = total;
    return 0;
}

