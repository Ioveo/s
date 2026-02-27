#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

# 文件操作模块源码
file_ops_c = """
#include "saia.h"

// ==================== 文件存在检查 ====================

int file_exists(const char *path) {
    if (!path) return 0;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && 
            !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
#endif
}

// ==================== 目录存在检查 ====================

int dir_exists(const char *path) {
    if (!path) return 0;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && 
            (attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

// ==================== 获取文件大小 ====================

size_t file_size(const char *path) {
    if (!path) return 0;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        return ((size_t)attrs.nFileSizeHigh << 32) | attrs.nFileSizeLow;
    }
    return 0;
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return 0;
#endif
}

// ==================== 删除文件 ====================

int file_remove(const char *path) {
    if (!path) return -1;
#ifdef _WIN32
    return DeleteFileA(path) ? 0 : -1;
#else
    return unlink(path);
#endif
}

// ==================== 创建目录 ====================

int dir_create(const char *path) {
    if (!path) return -1;
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) ? 0 : -1;
#else
    return mkdir(path, 0755);
#endif
}

// ==================== 读取所有内容 ====================

char* file_read_all(const char *path) {
    if (!path) return NULL;

    size_t size = file_size(path);
    if (size == 0) {
        // 可能是空文件或不存在
        char *empty = malloc(1);
        if (empty) empty[0] = '\\0';
        return empty;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(content, 1, size, fp);
    content[read] = '\\0';
    fclose(fp);

    return content;
}

// ==================== 写入全部内容 ====================

int file_write_all(const char *path, const char *content) {
    if (!path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);

    return (written == len) ? 0 : -1;
}

// ==================== 追加内容 ====================

int file_append(const char *path, const char *text) {
    if (!path || !text) return -1;

    FILE *fp = fopen(path, "a");
    if (!fp) return -1;

    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, fp);
    fclose(fp);

    return (written == len) ? 0 : -1;
}

// ==================== 文件轮转 ====================

int file_rotate(const char *path, size_t max_size, int backup_count) {
    if (!path || backup_count <= 0) return 0;

    size_t size = file_size(path);
    if (size <= max_size) return 0;

    // 删除最老的备份
    char old_path[MAX_PATH_LENGTH];
    snprintf(old_path, sizeof(old_path), "%s.%d", path, backup_count);
    file_remove(old_path);

    // 重命名备份文件
    for (int i = backup_count - 1; i >= 1; i--) {
        char src[MAX_PATH_LENGTH];
        char dst[MAX_PATH_LENGTH];
        snprintf(src, sizeof(src), "%s.%d", path, i);
        snprintf(dst, sizeof(dst), "%s.%d", path, i + 1);
#ifdef _WIN32
        MoveFileA(src, dst);
#else
        rename(src, dst);
#endif
    }

    // 重命名原文件
    snprintf(old_path, sizeof(old_path), "%s.1", path);
#ifdef _WIN32
    MoveFileA(path, old_path);
#else
    rename(path, old_path);
#endif

    return 0;
}

// ==================== 追加并自动轮转 ====================

int file_append_rotate(const char *path, const char *text, size_t max_size, int backup_count) {
    if (!path || !text) return -1;

    // 先检查是否需要轮转
    if (file_exists(path)) {
        file_rotate(path, max_size, backup_count);
    }

    return file_append(path, text);
}

// ==================== 读取所有行 ====================

int file_read_lines(const char *path, char ***lines, size_t *count) {
    if (!path || !lines || !count) return -1;

    *lines = NULL;
    *count = 0;

    char *content = file_read_all(path);
    if (!content) {
        if (file_exists(path)) return -1;
        return 0;  // 文件不存在，返回空列表
    }

    // 计算行数
    size_t line_count = 0;
    for (size_t i = 0; content[i]; i++) {
        if (content[i] == '\\n') line_count++;
    }
    if (strlen(content) > 0 && content[strlen(content)-1] != '\\n') {
        line_count++;
    }

    // 分配数组
    *lines = (char **)malloc(line_count * sizeof(char *));
    if (!*lines) {
        free(content);
        return -1;
    }

    // 分割行
    char *line = content;
    size_t idx = 0;
    while (line && idx < line_count) {
        char *newline = strchr(line, '\\n');
        if (newline) {
            *newline = '\\0';
            (*lines)[idx] = strdup(line);
            line = newline + 1;
        } else {
            (*lines)[idx] = strdup(line);
            line = NULL;
        }
        idx++;
    }

    *count = idx;
    free(content);

    return 0;
}

// ==================== 写入所有行 ====================

int file_write_lines(const char *path, char **lines, size_t count) {
    if (!path) return -1;

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    for (size_t i = 0; i < count; i++) {
        if (lines[i]) {
            fprintf(fp, "%s\\n", lines[i]);
        }
    }

    fclose(fp);
    return 0;
}
"""

# utils.c 源码
utils_c = """
#include "saia.h"

// ==================== 字符串缓冲区 ====================

string_buffer_t* string_buffer_create(size_t initial_size) {
    string_buffer_t *buf = (string_buffer_t *)malloc(sizeof(string_buffer_t));
    if (!buf) return NULL;

    buf->data = (char *)malloc(initial_size);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->data[0] = '\\0';
    buf->size = 0;
    buf->capacity = initial_size;
    return buf;
}

void string_buffer_free(string_buffer_t *buf) {
    if (!buf) return;
    if (buf->data) free(buf->data);
    free(buf);
}

int string_buffer_append(string_buffer_t *buf, const char *str) {
    if (!buf || !str) return -1;

    size_t len = strlen(str);
    if (buf->size + len + 1 > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        while (new_capacity < buf->size + len + 1) {
            new_capacity *= 2;
        }

        char *new_data = (char *)realloc(buf->data, new_capacity);
        if (!new_data) return -1;

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    memcpy(buf->data + buf->size, str, len + 1);
    buf->size += len;
    return 0;
}

int string_buffer_appendf(string_buffer_t *buf, const char *fmt, ...) {
    if (!buf || !fmt) return -1;

    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    return string_buffer_append(buf, temp);
}

char* string_buffer_to_string(string_buffer_t *buf) {
    if (!buf) return NULL;
    return strdup(buf->data);
}

// ==================== 字符串工具函数 ====================

char* str_trim(char *str) {
    if (!str) return NULL;

    // 去除前导空格
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    // 去除尾部空格
    char *end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\\0';

    return start;
}

char* str_lower(char *str) {
    if (!str) return NULL;
    for (size_t i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
    return str;
}

char* str_upper(char *str) {
    if (!str) return NULL;
    for (size_t i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
    return str;
}

int str_equals_ignore_case(const char *a, const char *b) {
    if (!a || !b) return (a == b);
    return strcasecmp(a, b) == 0;
}

char** str_split(const char *str, char delimiter, size_t *count) {
    if (!str || !count) return NULL;

    *count = 0;
    char **result = NULL;
    size_t capacity = 10;

    result = (char **)malloc(capacity * sizeof(char *));
    if (!result) return NULL;

    const char *start = str;
    const char *p = str;

    while (*p) {
        if (*p == delimiter || *p == '\\n' || *p == '\\r') {
            size_t len = p - start;
            if (len > 0) {
                result[*count] = (char *)malloc(len + 1);
                if (result[*count]) {
                    memcpy(result[*count], start, len);
                    result[*count][len] = '\\0';
                    (*count)++;

                    if (*count >= capacity) {
                        capacity *= 2;
                        result = (char **)realloc(result, capacity * sizeof(char *));
                    }
                }
            }
            start = p + 1;
        }
        p++;
    }

    // 处理最后一段
    if (start < p) {
        size_t len = p - start;
        result[*count] = (char *)malloc(len + 1);
        if (result[*count]) {
            memcpy(result[*count], start, len);
            result[*count][len] = '\\0';
            (*count)++;
        }
    }

    return result;
}

char* str_join(char **items, size_t count, const char *separator) {
    if (!items || count == 0) return strdup("");

    string_buffer_t *buf = string_buffer_create(1024);
    if (!buf) return NULL;

    for (size_t i = 0; i < count; i++) {
        if (i > 0 && separator) {
            string_buffer_append(buf, separator);
        }
        if (items[i]) {
            string_buffer_append(buf, items[i]);
        }
    }

    char *result = string_buffer_to_string(buf);
    string_buffer_free(buf);
    return result;
}

int str_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    return strstr(haystack, needle) != NULL;
}

int str_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int str_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (str_len < suffix_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

char* str_format(const char *fmt, ...) {
    if (!fmt) return NULL;
    
    char *result = NULL;
    va_list args;
    va_start(args, fmt);
    
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    
    if (len > 0) {
        result = (char *)malloc(len + 1);
        if (result) {
            va_start(args, fmt);
            vsnprintf(result, len + 1, fmt, args);
            va_end(args);
        }
    }
    
    return result;
}

// ==================== 时间工具函数 ====================

uint64_t get_current_time_ms(void) {
#ifdef _WIN32
    SYSTEMTIME st;
    FILETIME ft;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart / 10000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
#endif
}

void get_current_time_str(char *buf, size_t size) {
    if (!buf || size == 0) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// ==================== 系统资源工具 ====================

int get_available_memory_mb(void) {
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return (int)(statex.ullAvailPhys / (1024 * 1024));
#else
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return -1;
    
    char line[256];
    int mem_available = -1;
    
    while (fgets(line, sizeof(line), fp)) {
        if (str_starts_with(line, "MemAvailable:")) {
            sscanf(line, "MemAvailable: %d kB", &mem_available);
            mem_available /= 1024;  // 转换为MB
            break;
        }
    }
    
    fclose(fp);
    return mem_available;
#endif
}

int get_cpu_usage(void) {
#ifdef _WIN32
    static uint64_t last_idle = 0, last_total = 0;
    
    FILETIME idle_time, kernel_time, user_time;
    GetSystemTimes(&idle_time, &kernel_time, &user_time);
    
    uint64_t idle = ((uint64_t)idle_time.dwHighDateTime << 32) | idle_time.dwLowDateTime;
    uint64_t total = idle + (((uint64_t)kernel_time.dwHighDateTime << 32) | kernel_time.dwLowDateTime) +
                    (((uint64_t)user_time.dwHighDateTime << 32) | user_time.dwLowDateTime);
    
    if (last_total > 0) {
        uint64_t idle_diff = idle - last_idle;
        uint64_t total_diff = total - last_total;
        last_idle = idle;
        last_total = total;
        
        if (total_diff > 0) {
            return (int)(100 * (1.0 - (double)idle_diff / total_diff));
        }
    }
    
    last_idle = idle;
    last_total = total;
    return 0;
#else
    static uint64_t last_idle = 0, last_total = 0;
    
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return -1;
    
    char line[256];
    uint64_t user, nice, system, idle;
    
    if (fgets(line, sizeof(line), fp)) {
        sscanf(line, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
        
        uint64_t total = user + nice + system + idle;
        
        if (last_total > 0) {
            uint64_t idle_diff = idle - last_idle;
            uint64_t total_diff = total - last_total;
            
            last_idle = idle;
            last_total = total;
            
            if (total_diff > 0) {
                return (int)(100 * (1.0 - (double)idle_diff / total_diff));
            }
        }
        
        last_idle = idle;
        last_total = total;
    }
    
    fclose(fp);
    return 0;
#endif
}

int get_hostname(char *buf, size_t size) {
    if (!buf || size == 0) return -1;
#ifdef _WIN32
    return (gethostname(buf, size) == 0) ? 0 : -1;
#else
    return (gethostname(buf, size) == 0) ? 0 : -1;
#endif
}

pid_t get_current_pid(void) {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

int is_process_alive(pid_t pid) {
    if (pid <= 0) return 0;
#ifdef _WIN32
    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (handle) {
        DWORD exit_code;
        if (GetExitCodeProcess(handle, &exit_code)) {
            CloseHandle(handle);
            return (exit_code == STILL_ACTIVE);
        }
        CloseHandle(handle);
    }
    return 0;
#else
    return (kill(pid, 0) == 0);
#endif
}

int stop_process(pid_t pid) {
    if (!is_process_alive(pid)) return 0;
#ifdef _WIN32  
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (handle) {
        BOOL result = TerminateProcess(handle, 1);
        CloseHandle(handle);
        return result ? 0 : -1;
    }
    return -1;
#else
    return kill(pid, SIGTERM);
#endif
}
"""

# color.c 源码
color_c = """
#include "saia.h"

void color_reset(void) {
    printf("\\033[0m");
}

void color_bold(void) {
    printf("\\033[1m");
}

void color_blue(void) {
    printf("\\033[38;5;39m");
}

void color_cyan(void) {
    printf("\\033[38;5;51m");
}

void color_green(void) {
    printf("\\033[38;5;46m");
}

void color_yellow(void) {
    printf("\\033[38;5;214m");
}

void color_red(void) {
    printf("\\033[38;5;196m");
}

void color_magenta(void) {
    printf("\\033[38;5;201m");
}

void color_white(void) {
    printf("\\033[97m");
}

void color_dim(void) {
    printf("\\033[2m");
}
"""

# network.c 源码 - 简化版
network_c = """
#include "saia.h"

static int network_initialized = 0;

int network_init(void) {
    if (network_initialized) return 0;
    
    #ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return -1;
    }
    #endif
    
    network_initialized = 1;
    return 0;
}

void network_cleanup(void) {
    if (!network_initialized) return;
    
    #ifdef _WIN32
    WSACleanup();
    #endif
    
    network_initialized = 0;
}

int socket_create(int ipv6) {
    int domain = ipv6 ? AF_INET6 : AF_INET;
    int fd = socket(domain, SOCK_STREAM, 0);
    
    if (fd < 0) {
        return -1;
    }
    
    return fd;
}

int socket_close(int fd) {
    if (fd < 0) return -1;
    
    #ifdef _WIN32
    closesocket(fd);
    #else
    close(fd);
    #endif
    
    return 0;
}

int socket_set_nonblocking(int fd) {
    if (fd < 0) return -1;
    
    #ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode);
    #else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    #endif
}

int socket_set_timeout(int fd, int sec) {
    if (fd < 0) return -1;
    
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    
    return 0;
}

int ip_parse(const char *str, ip_addr_t *addr) {
    if (!str || !addr) return -1;
    
    memset(addr, 0, sizeof(ip_addr_t));
    
    if (strstr(str, ":")) {
        addr->is_ipv6 = 1;
        strncpy(addr->ip, str, sizeof(addr->ip) - 1);
        return 0;
    }
    
    if (inet_pton(AF_INET, str, &addr->ip_int) <= 0) {
        return -1;
    }
    
    strncpy(addr->ip, str, sizeof(addr->ip) - 1);
    return 0;
}

int ip_to_string(const ip_addr_t *addr, char *buf, size_t size) {
    if (!addr || !buf || size == 0) return -1;
    
    strncpy(buf, addr->ip, size - 1);
    buf[size - 1] = '\\0';
    return 0;
}

int ip_is_valid(const char *str) {
    if (!str) return 0;
    
    ip_addr_t addr;
    return ip_parse(str, &addr) == 0;
}

int socket_connect_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, int timeout_ms) {
    socket_set_nonblocking(fd);
    
    int result = connect(fd, addr, addrlen);
    
    if (result == 0) {
        return 0;
    }
    
    #ifdef _WIN32
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
        return -1;
    }
    #else
    if (errno != EINPROGRESS) {
        return -1;
    }
    #endif
    
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(fd, &write_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    result = select(fd + 1, NULL, &write_fds, NULL, &tv);
    
    if (result <= 0) {
        return -1;
    }
    
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
    
    return error ? -1 : 0;
}

int socket_send_all(int fd, const char *data, size_t len, int timeout_ms) {
    if (fd < 0 || !data || len == 0) return -1;
    
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(fd, &write_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (select(fd + 1, NULL, &write_fds, NULL, &tv) <= 0) {
        return -1;
    }
    
    return send(fd, data, len, 0);
}

int socket_recv_until(int fd, char *buf, size_t size, const char *delimiter, int timeout_ms) {
    if (fd < 0 || !buf || size == 0) return -1;
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (select(fd + 1, &read_fds, NULL, NULL, &tv) <= 0) {
        return -1;
    }
    
    int received = recv(fd, buf, size - 1, 0);
    if (received <= 0) return -1;
    
    buf[received] = '\\0';
    return received;
}
"""

# scanner.c 源码 - 框架
scanner_c = """
#include "saia.h"

static int scanner_initialized = 0;

int scanner_init(void) {
    if (scanner_initialized) return 0;
    
    scanner_initialized = 1;
    return 0;
}

void scanner_cleanup(void) {
    if (!scanner_initialized) return;
    
    scanner_initialized = 0;
}

int scanner_run(scan_target_t *target) {
    if (!target) return -1;
    
    printf("扫描器运行中...\\n");
    return 0;
}

int scanner_scan_port(ip_port_t addr, scan_result_t *result) {
    if (!result) return -1;
    
    memset(result, 0, sizeof(scan_result_t));
    result->addr = addr;
    result->scan_time = time(NULL);
    
    return 0;
}

int scanner_verify_xui(ip_port_t addr, credential_t *cred, scan_result_t *result) {
    return 0;
}

int scanner_verify_socks5(ip_port_t addr, credential_t *cred, scan_result_t *result) {
    return 0;
}

int scanner_scan_callback(scan_result_t *result, void *user_data) {
    (void)result;
    (void)user_data;
    return 0;
}
"""

# backpressure.c 源码 - 新增压背功能
backpressure_c = """
#include "saia.h"

int backpressure_init(backpressure_state_t *state) {
    if (!state) return -1;
    
    memset(state, 0, sizeof(backpressure_state_t));
    state->enabled = 1;
    state->cpu_threshold = DEFAULT_BACKPRESSURE_THRESHOLD * 100.0f;
    state->mem_threshold = 2048.0f;
    state->current_connections = 0;
    state->max_connections = MAX_CONCURRENT_CONNECTIONS;
    state->last_check = 0;
    state->is_throttled = 0;
    state->current_cpu = 0.0f;
    state->current_mem = (float)get_available_memory_mb();
    
    return 0;
}

void backpressure_update(backpressure_state_t *state) {
    if (!state || !state->enabled) return;
    
    time_t now = time(NULL);
    if (now - state->last_check < BACKPRESSURE_CHECK_INTERVAL) {
        return;
    }
    
    state->current_cpu = (float)get_cpu_usage();
    state->current_mem = (float)get_available_memory_mb();
    state->last_check = now;
    
    // 检查是否达到阈值
    int cpu_high = (state->current_cpu > state->cpu_threshold);
    int mem_low = (state->current_mem < state->mem_threshold);
    
    // 内存使用 = 总内存 - 可用内存
    float mem_usage = 0.0f;
    #ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    mem_usage = (float)(statex.ullTotalPhys - statex.ullAvailPhys) / (1024 * 1024);
    #endif
    
    if (mem_usage > state->mem_threshold) {
        mem_low = 1;
    }
    
    state->is_throttled = (cpu_high || mem_low || 
                          state->current_connections > state->max_connections);
    
    // 自动调整最大连接数
    if (state->is_throttled && state->max_connections > MIN_CONCURRENT_CONNECTIONS) {
        state->max_connections -= 50;
        if (state->max_connections < MIN_CONCURRENT_CONNECTIONS) {
            state->max_connections = MIN_CONCURRENT_CONNECTIONS;
        }
    } else if (!state->is_throttled && state->max_connections < MAX_CONCURRENT_CONNECTIONS) {
        state->max_connections += 20;
    }
}

int backpressure_should_throttle(backpressure_state_t *state) {
    if (!state || !state->enabled) return 0;
    return state->is_throttled;
}

void backpressure_adjust_connections(backpressure_state_t *state, int *current_conn) {
    if (!state || !current_conn) return;
    
    if (*current_conn > state->max_connections) {
        *current_conn = state->max_connections;
    }
    
    state->current_connections = *current_conn;
}
"""

# 写入文件
files = {
    "file_ops.c": file_ops_c,
    "utils.c": utils_c,
    "color.c": color_c,
    "network.c": network_c,
    "scanner.c": scanner_c,
    "backpressure.c": backpressure_c,
}

for filename, content in files.items():
    with open(f"F:\\C\\{filename}", "w", encoding="utf-8") as f:
        f.write(content)
    print(f"已创建: {filename}")
