#include "saia.h"

static volatile int running_threads = 0;
#ifdef _WIN32
static HANDLE lock_stats;
static HANDLE lock_file;
#define MUTEX_LOCK(x) WaitForSingleObject(x, INFINITE)
#define MUTEX_UNLOCK(x) ReleaseMutex(x)
#define MUTEX_INIT(x) x = CreateMutex(NULL, FALSE, NULL)
#else
static pthread_mutex_t lock_stats = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_file = PTHREAD_MUTEX_INITIALIZER;
#define MUTEX_LOCK(x) pthread_mutex_lock(&x)
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&x)
#define MUTEX_INIT(x) // Static init is enough
#endif

// 初始化锁
void init_locks() {
    #ifdef _WIN32
    MUTEX_INIT(lock_stats);
    MUTEX_INIT(lock_file);
    #endif
}

// ==================== 验证逻辑: SOCKS5 ====================

int verify_socks5(const char *ip, uint16_t port, const char *user, const char *pass, int timeout_ms) {
    int fd = socket_create(0);
    if (fd < 0) return 0;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (socket_connect_timeout(fd, (struct sockaddr*)&addr, sizeof(addr), timeout_ms) != 0) {
        socket_close(fd);
        return 0;
    }
    
    // 1. 初始握手
    char handshake[] = {0x05, 0x01, 0x00}; // 无认证
    if (user && pass && strlen(user) > 0) {
        handshake[2] = 0x02; // 用户名密码认证
    }
    
    if (socket_send_all(fd, handshake, sizeof(handshake), timeout_ms) < 0) {
        socket_close(fd);
        return 0;
    }
    
    char resp[2];
    if (socket_recv_until(fd, resp, 2, NULL, timeout_ms) != 2) {
        socket_close(fd);
        return 0;
    }
    
    if (resp[0] != 0x05) {
        socket_close(fd);
        return 0;
    }
    
    // 2. 认证 (如果需要)
    if (resp[1] == 0x02) {
        if (!user || !pass) {
            socket_close(fd);
            return 0;
        }
        
        char auth_buf[600]; /* max: 1+1+255+1+255 = 513 bytes for SOCKS5 auth */
        int ulen = (int)strlen(user);
        int plen = (int)strlen(pass);
        /* SOCKS5 RFC 1929: username/password max length is 255 */
        if (ulen > 255) ulen = 255;
        if (plen > 255) plen = 255;
        int idx = 0;
        
        auth_buf[idx++] = 0x01; // Version
        auth_buf[idx++] = (char)ulen;
        memcpy(auth_buf + idx, user, ulen); idx += ulen;
        auth_buf[idx++] = (char)plen;
        memcpy(auth_buf + idx, pass, plen); idx += plen;
        
        if (socket_send_all(fd, auth_buf, idx, timeout_ms) < 0) {
            socket_close(fd);
            return 0;
        }
        
        char auth_resp[2];
        if (socket_recv_until(fd, auth_resp, 2, NULL, timeout_ms) != 2) {
            socket_close(fd);
            return 0;
        }
        
        if (auth_resp[1] != 0x00) { // 认证失败
            socket_close(fd);
            return 0;
        }
    } else if (resp[1] != 0x00) {
        socket_close(fd);
        return 0; // 不支持的认证方法
    }
    
    // 3. 发送CONNECT请求 (验证是否真通)
    // 连接 Google DNS (8.8.8.8:53) 作为测试
    char connect_req[] = {
        0x05, 0x01, 0x00, 0x01, // VER, CMD, RSV, ATYP(IPv4)
        0x08, 0x08, 0x08, 0x08, // 8.8.8.8
        0x00, 0x35              // Port 53
    };
    
    if (socket_send_all(fd, connect_req, sizeof(connect_req), timeout_ms) < 0) {
        socket_close(fd);
        return 0;
    }
    
    char conn_resp[10];
    if (socket_recv_until(fd, conn_resp, 4, NULL, timeout_ms) < 4) { // 只读前4字节判断状态
        socket_close(fd);
        return 0;
    }
    
    socket_close(fd);
    
    // 响应状态 0x00 = succeeded
    return (conn_resp[1] == 0x00);
}

// ==================== 验证逻辑: XUI ====================

int verify_xui(const char *ip, uint16_t port, const char *user, const char *pass, int timeout_ms) {
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/login", ip, port);
    
    char data[512];
    snprintf(data, sizeof(data), "username=%s&password=%s", user, pass);
    
    http_response_t *res = http_post(url, data, timeout_ms);
    if (!res) return 0;
    
    int success = 0;
    
    // 检查响应
    if (res->status_code == 200) {
        // 检查是否有 success: true (JSON)
        if (res->body && strstr(res->body, "\"success\":true")) {
            success = 1;
        }
        // 或者检查是否有重定向/Cookie设置 (部分版本)
        if (res->headers && strstr(res->headers, "Set-Cookie")) {
            success = 1;
        }
    }
    
    http_response_free(res);
    return success;
}

// ==================== 多线程调度 ====================

// 线程参数
typedef struct {
    char ip[64];
    uint16_t port;
    credential_t *creds;
    size_t cred_count;
    int work_mode;
} worker_arg_t;

// 工作线程函数
#ifdef _WIN32
unsigned __stdcall worker_thread(void *arg) {
#else
void *worker_thread(void *arg) {
#endif
    worker_arg_t *task = (worker_arg_t *)arg;
    int found = 0;
    (void)found; /* suppress unused-variable warning */
    
    // 更新扫描统计
    MUTEX_LOCK(lock_stats);
    g_state.total_scanned++;
    MUTEX_UNLOCK(lock_stats);
    
    // 1. 端口连通性检查
    int fd = socket_create(0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(task->port);
    inet_pton(AF_INET, task->ip, &addr.sin_addr);
    
    if (socket_connect_timeout(fd, (struct sockaddr*)&addr, sizeof(addr), 1000) != 0) {
        socket_close(fd);
        free(task);
        
        MUTEX_LOCK(lock_stats);
        running_threads--;
        MUTEX_UNLOCK(lock_stats);
        
        #ifdef _WIN32
        return 0;
        #else
        return NULL;
        #endif
    }
    socket_close(fd);
    
    // 端口开放，开始验证
    if (task->work_mode == MODE_S5) {
        for (size_t i = 0; i < task->cred_count; i++) {
            if (verify_socks5(task->ip, task->port, task->creds[i].username, task->creds[i].password, 3000)) {
                found = 1;
                
                MUTEX_LOCK(lock_stats);
                g_state.total_found++;
                g_state.total_verified++;
                MUTEX_UNLOCK(lock_stats);
                
                // 记录结果
                char result_line[1024];
                snprintf(result_line, sizeof(result_line), 
                         "[S5_VERIFIED] %s:%d %s:%s", 
                         task->ip, task->port, 
                         task->creds[i].username, task->creds[i].password);
                         
                MUTEX_LOCK(lock_file);
                file_append(g_config.report_file, result_line);
                file_append(g_config.report_file, "\n");
                printf("\n%s\n", result_line); // 实时显示
                MUTEX_UNLOCK(lock_file);
                
                break; // 找到一个密码就停止
            }
        }
    } else if (task->work_mode == MODE_XUI) {
        // 简化版：仅尝试第一组凭据
        if (task->cred_count > 0) {
             if (verify_xui(task->ip, task->port, task->creds[0].username, task->creds[0].password, 3000)) {
                 MUTEX_LOCK(lock_stats);
                 g_state.total_found++;
                 MUTEX_UNLOCK(lock_stats);
                 
                 char result_line[1024];
                 snprintf(result_line, sizeof(result_line), "[XUI_FOUND] %s:%d", task->ip, task->port);
                 MUTEX_LOCK(lock_file);
                 file_append(g_config.report_file, result_line);
                 file_append(g_config.report_file, "\n");
                 MUTEX_UNLOCK(lock_file);
             }
        }
    }
    
    free(task);
    
    MUTEX_LOCK(lock_stats);
    running_threads--;
    MUTEX_UNLOCK(lock_stats);
    
    #ifdef _WIN32
    return 0;
    #else
    return NULL;
    #endif
}

// 启动扫描
void scanner_start_multithreaded(char **nodes, size_t node_count, credential_t *creds, size_t cred_count, uint16_t *ports, size_t port_count) {
    init_locks();
    printf("开始扫描... 总节点: %zu, 总端口: %zu\n", node_count, port_count);
    
    size_t node_idx = 0;
    
    while (g_running && node_idx < node_count) {
        // 压背控制
        if (g_config.backpressure.enabled) {
            backpressure_update(&g_config.backpressure);
            if (backpressure_should_throttle(&g_config.backpressure)) {
                saia_sleep(1000);
                continue;
            }
        }
        
        // 检查并发数
        MUTEX_LOCK(lock_stats);
        int current = running_threads;
        MUTEX_UNLOCK(lock_stats);
        
        if (current >= g_config.threads) {
            saia_sleep(100);
            continue;
        }
        
        // 创建任务
        for (size_t p = 0; p < port_count; p++) {
            worker_arg_t *arg = (worker_arg_t *)malloc(sizeof(worker_arg_t));
            strncpy(arg->ip, nodes[node_idx], sizeof(arg->ip) - 1);
            arg->port = ports[p];
            arg->creds = creds;
            arg->cred_count = cred_count;
            arg->work_mode = g_config.mode;
            
            MUTEX_LOCK(lock_stats);
            running_threads++;
            MUTEX_UNLOCK(lock_stats);
            
            #ifdef _WIN32
            _beginthreadex(NULL, 0, worker_thread, arg, 0, NULL);
            #else
            pthread_t tid;
            pthread_create(&tid, NULL, worker_thread, arg);
            pthread_detach(tid);
            #endif
        }
        
        node_idx++;
        
        // 简单的进度显示
        if (node_idx % 10 == 0) {
            printf("\r进度: %zu/%zu (线程: %d) ", node_idx, node_count, current);
            fflush(stdout);
        }
    }
    
    // 等待剩余线程
    while (1) {
        MUTEX_LOCK(lock_stats);
        int remaining = running_threads;
        MUTEX_UNLOCK(lock_stats);
        if (remaining <= 0) break;
        saia_sleep(500);
    }
    
    printf("\n扫描结束\n");
}

// 占位符接口实现
int scanner_init(void) { return 0; }
void scanner_cleanup(void) { }
// scanner_run 已经被新的逻辑替代，这里仅保留兼容性
int scanner_run(scan_target_t *target) { 
    (void)target;
    return 0; 
}
int scanner_scan_port(ip_port_t addr, scan_result_t *result) { (void)addr; (void)result; return 0; }
int scanner_scan_callback(scan_result_t *result, void *user_data) { (void)result; (void)user_data; return 0; }
