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
            
            if (strnicmp(asn_start, "AS", 2) == 0) {
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
        p = strchr(p, "|");
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
