
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

    buf->data[0] = '\0';
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
    *(end + 1) = '\0';

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
        if (*p == delimiter || *p == '\n' || *p == '\r') {
            size_t len = p - start;
            if (len > 0) {
                result[*count] = (char *)malloc(len + 1);
                if (result[*count]) {
                    memcpy(result[*count], start, len);
                    result[*count][len] = '\0';
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
            result[*count][len] = '\0';
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
