#include "saia.h"

#include <locale.h>

// 全局变量

config_t g_config;

state_t g_state = {0};

volatile sig_atomic_t g_running = 1;

volatile sig_atomic_t g_reload = 0;

// ==================== 信号处理 ====================

#ifdef _WIN32

BOOL WINAPI saia_console_handler(DWORD dwCtrlType) {

    switch (dwCtrlType) {

        case CTRL_C_EVENT:

        case CTRL_CLOSE_EVENT:

        case CTRL_BREAK_EVENT:

            g_running = 0;

            printf("\n[INT] 收到中断信号，正在停止...\n");

            return TRUE;

        default:

            return FALSE;

    }

}

void saia_signal_handler(int signum) {

    (void)signum;

    g_running = 0;

}

#else

void saia_signal_handler(int signum) {

    switch (signum) {

        case SIGINT:

        case SIGTERM:

            g_running = 0;

            printf("\n[INT] 收到信号 %d，正在停止...\n", signum);

            break;

        case SIGHUP:

            g_reload = 1;

            printf("[INT] 收到重载信号\n");

            break;

    }

}

#endif

// ==================== 清理函数 ====================

void saia_cleanup(void) {

    printf("\n");

    printf("========================================\n");

    printf("  SAIA 正在清理资源...\n");

    printf("========================================\n");

    network_cleanup();

    scanner_cleanup();

    printf("清理完成. 再见!\n");

}

// ==================== 打印横幅 ====================

void saia_print_banner(void) {

    color_blue();

    color_bold();

    printf("\n");

    printf("╔════════════════════════════════════════╗\n");

    printf("║  " SAIA_NAME " v" SAIA_VERSION "     ║\n");

    printf("║              纯C实现版本                 ║\n");

    printf("║          自包含 - 无第三方依赖         ║\n");

    printf("╚════════════════════════════════════════╝\n");

    color_reset();

    printf("\n");

}

// ==================== 打印统计信息 ====================

void saia_print_stats(state_t *state) {

    char time_str[64];

    time_t elapsed = time(NULL) - state->start_time;

    int hours = elapsed / 3600;

    int minutes = (elapsed % 3600) / 60;

    int seconds = elapsed % 60;

    color_cyan();

    color_bold();

    printf("\n【运行统计】\n");

    color_reset();

    printf("  运行时间: %02d:%02d:%02d\n", hours, minutes, seconds);

    printf("  模式: %s (%s)\n", state->mode == 1 ? "XUI专项" :\n\nstate->mode == 2 ? "S5专项" :\n\nstate->mode == 3 ? "深度全能" : "验真模式", state->work_mode == 1 ? "探索" :\n\nstate->work_mode == 2 ? "探索+验真" : "只留极品");

    printf("  并发线程: %d\n", state->threads);

    printf("  已扫描: %llu\n", (unsigned long long)state->total_scanned);

    printf("  已发现: %llu\n", (unsigned long long)state->total_found);

    printf("  已验证: %llu\n", (unsigned long long)state->total_verified);

    if (g_config.backpressure.enabled) {

        color_yellow();

        printf("\n【压背状态】\n");

        color_reset();

        printf("  当前CPU: %.1f%%\n", g_config.backpressure.current_cpu);

        printf("  当前内存: %.1f MB\n", g_config.backpressure.current_mem);

        printf("  当前连接: %d/%d\n", g_config.backpressure.current_connections, g_config.backpressure.max_connections);

        printf("  限流状态: %s\n", g_config.backpressure.is_throttled ? "已限流" : "正常");

    }

    printf("\n");

}

// ==================== 交互式菜单 ====================

int saia_print_menu(void) {

    color_cyan();

    color_bold();

    printf("═ 功能菜单 ═\n");

    color_reset();

    color_white();

    printf("  [1] 开始审计扫描\n");

    printf("  [2] 配置参数\n");

    printf("  [3] 查看报表\n");

    printf("  [4] 导出结果\n");

    printf("  [5] 节点管理\n");

    printf("  [6] 凭据管理\n");

    printf("  [7] Telegram推送\n");

    printf("  [8] 压背控制\n");  // 新增\n
    printf("  [9] 清理数据\n");

    printf("  [0] 退出\n");

    color_reset();

    printf("\n请选择: ");

    fflush(stdout);

    int choice;

    if (scanf("%d", &choice) != 1) {

        while (getchar() != '\n');  // 清除输入缓冲区

        return -1;

    }

    while (getchar() != '\n');  // 清除换行符

    return choice;

}

// ==================== 开始审计 ====================

int saia_run_audit_internal(int auto_mode, int auto_scan_mode, int auto_threads) {

    char input[256];

    int mode, scan_mode, threads;

    char ports_raw[1024] = {0};

    color_yellow();

    printf("\n【审计配置】\n");

    color_reset();

    if (auto_mode > 0) {

        mode = auto_mode;

        scan_mode = auto_scan_mode;

        threads = auto_threads;

    } else {

    // 选择模式

    printf("\n模式选择:\n");

    printf("  1. XUI专项 (扫描XUI面板)\n");

    printf("  2. S5专项 (扫描SOCKS5代理)\n");

    printf("  3. 深度全能 (全面扫描)\n");

    printf("  4. 验真模式 (只验证已知节点)\n");

    printf("模式 [1-4]: ");

    fflush(stdout);

    scanf("%d", &mode);

    // 选择扫描模式

    printf("\n扫描模式:\n");

    printf("  1. 探索 (只扫描存活)\n");

    printf("  2. 探索+验真 (扫描并验证)\n");

    printf("  3. 只留极品 (只保留可用)\n");

    printf("模式 [1-3]: ");

    fflush(stdout);

    scanf("%d", &scan_mode);

    // 线程数

    printf("\n并发线程数 [50-2000]: ");

    fflush(stdout);

    scanf("%d", &threads);

    if (threads < MIN_CONCURRENT_CONNECTIONS) threads = MIN_CONCURRENT_CONNECTIONS;

    if (threads > MAX_THREADS) threads = MAX_THREADS;

    // 端口

    printf("\n端口配置 (留空使用默认): ");

    fflush(stdout);

    while (getchar() != '\n');  // 清除之前的输入

    if (fgets(input, sizeof(input), stdin)) {

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) > 0) {

            strncpy(ports_raw, input, sizeof(ports_raw) - 1);

        }

    }

    // 压背控制

    printf("\n启用压背控制? [Y/n]: ");

    fflush(stdout);

    if (fgets(input, sizeof(input), stdin)) {

        g_config.backpressure.enabled = (toupper(input[0]) != 'N');

    }

    if (g_config.backpressure.enabled) {

        printf("CPU阈值 [%%] [80]: ");

        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {

            g_config.backpressure.cpu_threshold = atof(input);

        } else {

            g_config.backpressure.cpu_threshold = 80.0;

        }

        printf("内存阈值 [MB] [2048]: ");

        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {

            g_config.backpressure.mem_threshold = atof(input);

        } else {

            g_config.backpressure.mem_threshold = 2048.0;

        }

        g_config.backpressure.max_connections = threads;

    }

    }

    // 保存配置

    g_config.mode = mode;

    g_config.scan_mode = scan_mode;

    g_config.threads = threads;

    g_config.timeout = DEFAULT_TIMEOUT;

    // 更新状态

    strncpy(g_state.ports_raw, ports_raw, sizeof(g_state.ports_raw) - 1);

    g_state.mode = mode;

    g_state.work_mode = scan_mode;

    g_state.threads = threads;

    color_green();

    printf("\n配置完成，开始扫描...\n");

    color_reset();

    // 初始化

    if (network_init() != 0) {

        color_red();

        printf("错误: 网络初始化失败\n");

        color_reset();

        return -1;

    }

    if (scanner_init() != 0) {

        color_red();

        printf("错误: 扫描器初始化失败\n");

        color_reset();

        network_cleanup();

        return -1;

    }

    // 读取数据文件

    char **nodes = NULL;

    size_t node_count = 0;

    if (file_read_lines(g_config.nodes_file, &nodes, &node_count) != 0 || node_count == 0) {

        // 尝试备用文件

        file_read_lines("ip.txt", &nodes, &node_count);

    }

    if (!nodes || node_count == 0) {

        color_red();

        printf("错误: 没有找到目标节点 (请检查 nodes.list 或 ip.txt)\n");

        color_reset();

        return -1;

    }

    char **raw_tokens = NULL;

    size_t token_lines = 0;

    file_read_lines(g_config.tokens_file, &raw_tokens, &token_lines);

    if (!raw_tokens) {

        file_read_lines("pass.txt", &raw_tokens, &token_lines);

    }

    credential_t *creds = NULL;

    size_t cred_count = 0;

    if (raw_tokens && token_lines > 0) {

        creds = (credential_t *)malloc(token_lines * sizeof(credential_t));

        for (size_t i = 0; i < token_lines; i++) {

            if (parse_credentials(raw_tokens[i], &creds[cred_count]) == 0) {

                cred_count++;

            }

            free(raw_tokens[i]);

        }

        free(raw_tokens);

    } else {

        // 默认凭据

        creds = (credential_t *)malloc(sizeof(credential_t));

        strcpy(creds[0].username, "admin");

        strcpy(creds[0].password, "admin");

        cred_count = 1;

    }

    // 解析端口

    uint16_t *ports = NULL;

    size_t port_count = 0;

    if (strlen(ports_raw) > 0) {

        config_parse_ports(ports_raw, &ports, &port_count);

    } else {

        config_set_default_ports(g_config.mode, &ports, &port_count);

    }

    // 开始扫描

    saia_print_banner();

    time(&g_state.start_time);

    strcpy(g_state.status, "running");

    g_state.pid = get_current_pid();

    g_state.total_scanned = 0;

    g_state.total_found = 0;

    g_state.total_verified = 0;

    // 启动多线程扫描

    scanner_start_multithreaded(nodes, node_count, creds, cred_count, ports, port_count);

    strcpy(g_state.status, "completed");

    // 清理数据

    for (size_t i = 0; i < node_count; i++) free(nodes[i]);

    free(nodes);

    if (creds) free(creds);

    if (ports) free(ports);

    // 清理模块

    scanner_cleanup();

    network_cleanup();

    return 0;

}

// ==================== 配置菜单 ====================

int saia_config_menu(void) {

    color_yellow();

    printf("\n【配置参数】\n");

    color_reset();

    printf("当前配置:\n");

    printf("  工作模式: %d\n", g_config.mode);

    printf("  扫描模式: %d\n", g_config.scan_mode);

    printf("  并发线程: %d\n", g_config.threads);

    printf("  超时: %d 秒\n", g_config.timeout);

    printf("  Verbose: %s\n", g_config.verbose ? "是" : "否");

    printf("  暴露密钥: %s\n", g_config.expose_secret ? "是" : "否");

    return 0;

}

// ==================== 报表菜单 ====================

int saia_report_menu(void) {

    color_yellow();

    printf("\n【查看报表】\n");

    color_reset();

    char report_path[MAX_PATH_LENGTH];

    snprintf(report_path, sizeof(report_path), "%s\\audit_report.log",

             g_config.base_dir);

    if (file_exists(report_path)) {

        size_t size = file_size(report_path);

        printf("报告文件: %s\n", report_path);

        printf("文件大小: %.2f MB\n", size / (1024.0 * 1024.0));

        printf("\n最近100行:\n");

        printf("----------------------------------------\n");

        char **lines = NULL;

        size_t count = 0;

        if (file_read_lines(report_path, &lines, &count) == 0) {

            size_t start = (count > 100) ? count - 100 : 0;

            const char *filter_keyword = "";

            for (size_t i = start; i < count; i++) {

                if (strlen(filter_keyword) > 0) {

                    if (strstr(lines[i], filter_keyword) == NULL) {

                        continue;

                    }

                }

                printf("%s\n", lines[i]);

            }

            for (size_t i = 0; i < count; i++) {

                free(lines[i]);

            }

            free(lines);

        }

        printf("----------------------------------------\n");

        printf("总计: %zu 行\n", count);

    } else {

        printf("暂无报告文件\n");

    }

    return 0;

}

// ==================== 辅助输入函数 ====================

static int saia_write_list_file_from_input(const char *file_path, int split_spaces, int append_mode) {

    color_cyan();

    printf("支持空格/换行混合输入 (自动去除 # 注释)\n");

    color_reset();

    printf("请输入内容，单独输入 EOF 结束:\n");

    char buffer[4096];

    char tmp_path[4096];

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", file_path);

    FILE *fp;

    if (append_mode) {

        fp = fopen(tmp_path, "w");

        if (fp && file_exists(file_path)) {

            char *content = file_read_all(file_path);

            if (content) {

                fprintf(fp, "%s", content);

                size_t len = strlen(content);

                if (len > 0 && content[len - 1] != '\n') {

                    fprintf(fp, "\n");

                }

                free(content);

            }

        }

    } else {

        fp = fopen(tmp_path, "w");

    }

    if (!fp) {

        color_red();

        printf(">>> 写入临时文件失败\n");

        color_reset();

        return -1;

    }

    int count = 0;

    while (fgets(buffer, sizeof(buffer), stdin)) {

        char *comment = strchr(buffer, '#');

        if (comment) *comment = '\0';

        buffer[strcspn(buffer, "\n")] = '\0';

        char *trimmed = str_trim(buffer);

        if (!trimmed) continue;

        if (strcmp(trimmed, "EOF") == 0) {

            break;

        }

        if (split_spaces) {

            char *token = strtok(trimmed, " \t\n");

            while (token != NULL) {

                if (strlen(token) > 0) {

                    fprintf(fp, "%s\n", token);

                    count++;

                }

                token = strtok(NULL, " \t\n");

            }

        } else {

            if (strlen(trimmed) > 0) {

                fprintf(fp, "%s\n", trimmed);

                count++;

            }

        }

    }

    fclose(fp);

    if (file_exists(file_path)) {

        file_remove(file_path);

    }

    rename(tmp_path, file_path);

    return count;

}

// ==================== 节点管理 ====================

int saia_nodes_menu(void) {

    color_yellow();

    printf("\n【节点管理】\n");

    color_reset();

    printf("  [1] 添加节点\n");

    printf("  [2] 查看节点\n");

    printf("  [3] 导入IP库\n");

    printf("  [0] 返回\n");

    printf("选择: ");

    fflush(stdout);

    int choice;

    scanf("%d", &choice);

    while (getchar() != '\n');

    char nodes_path[MAX_PATH_LENGTH];

    snprintf(nodes_path, sizeof(nodes_path), "%s\\nodes.list",

             g_config.base_dir);

    switch (choice) {

        case 1: {

            color_cyan();

            printf(">>> 更换 IP 列表\n");

            color_reset();

            int count = saia_write_list_file_from_input(nodes_path, 1, 0);

            if (count >= 0) {

                color_green();

                printf(">>> IP 列表已更新，本次写入 %d 条\n\n", count);

                color_reset();

            } else {

                color_red();

                printf(">>> 写入失败或已取消\n");

                color_reset();

            }

            break;

        }

        case 2: {

            color_cyan();

            printf(">>> 添加 IP 节点\n");

            color_reset();

            int count = saia_write_list_file_from_input(nodes_path, 1, 1);

            if (count >= 0) {

                color_green();

                printf(">>> 节点已追加，本次追加 %d 条\n\n", count);

                color_reset();

            } else {

                color_red();

                printf(">>> 追加失败或已取消\n");

                color_reset();

            }

            break;

        }

        case 3: {

            if (file_exists(nodes_path)) {

                char **lines = NULL;

                size_t count = 0;

                if (file_read_lines(nodes_path, &lines, &count) == 0) {

                    printf("共 %zu 个节点:\n", count);

                    for (size_t i = 0; i < count && i < 20; i++) {

                        printf("  %s\n", lines[i]);

                    }

                    if (count > 20) {

                        printf("  ... 还有 %zu 个\n", count - 20);

                    }

                    for (size_t i = 0; i < count; i++) {

                        free(lines[i]);

                    }

                    free(lines);

                }

            } else {

                printf("暂无节点\n");

            }

            break;

        }

        case 4:

            printf("功能开发中...\n");

            break;

        default:

            return 0;

    }

    return 0;

}

// ==================== 凭据管理 ====================

int saia_credentials_menu(void) {

    color_yellow();

    printf("\n【凭据管理】\n");

    color_reset();

    char tokens_path[MAX_PATH_LENGTH];

    snprintf(tokens_path, sizeof(tokens_path), "%s\\tokens.list",

             g_config.base_dir);

    printf("  [1] 添加凭据 (用户名:密码)\n");

    printf("  [2] 查看凭据\n");

    printf("  [0] 返回\n");

    printf("选择: ");

    fflush(stdout);

    int choice;

    scanf("%d", &choice);

    while (getchar() != '\n');

    switch (choice) {

        case 1: {

            color_cyan();

            printf(">>> 更新 Tokens\n");

            color_reset();

            int count = saia_write_list_file_from_input(tokens_path, 1, 0);

            if (count >= 0) {

                color_green();

                printf(">>> Tokens 已更新，本次写入 %d 条\n\n", count);

                color_reset();

            } else {

                color_red();

                printf(">>> 写入失败或已取消\n");

                color_reset();

            }

            break;

        }

        case 2: {

            color_cyan();

            printf(">>> 添加 Tokens\n");

            color_reset();

            int count = saia_write_list_file_from_input(tokens_path, 1, 1);

            if (count >= 0) {

                color_green();

                printf(">>> Tokens 已追加，本次追加 %d 条\n\n", count);

                color_reset();

            } else {

                color_red();

                printf(">>> 追加失败或已取消\n");

                color_reset();

            }

            break;

        }

        case 3: {

            if (file_exists(tokens_path)) {

                char **lines = NULL;

                size_t count = 0;

                if (file_read_lines(tokens_path, &lines, &count) == 0) {

                    printf("共 %zu 个凭据:\n", count);

                    for (size_t i = 0; i < count && i < 10; i++) {

                        if (strchr(lines[i], ':')) {

                            char *colon = strchr(lines[i], ':');

                            *colon = '\0';

                            printf("  用户: %s, 密码: ***\n", lines[i]);

                        }

                    }

                    if (count > 10) {

                        printf("  ... 还有 %zu 个\n", count - 10);

                    }

                    for (size_t i = 0; i < count; i++) {

                        free(lines[i]);

                    }

                    free(lines);

                }

            } else {

                printf("暂无凭据\n");

            }

            break;

        }

        default:

            return 0;

    }

    return 0;

}

// ==================== Telegram菜单 ====================

int saia_telegram_menu(void) {

    color_yellow();

    printf("\n【Telegram推送】\n");

    color_reset();

    printf("当前状态: %s\n", g_config.telegram_enabled ? "已启用" : "已禁用");

    if (g_config.telegram_enabled) {

        printf("Bot Token: %s\n", g_config.telegram_token);

        printf("Chat ID: %s\n", g_config.telegram_chat_id);

        printf("推送间隔: %d 分钟\n", g_config.telegram_interval);

    }

    printf("\n  [1] 启用/禁用\n");

    printf("  [2] 配置Bot\n");

    printf("  [3] 测试推送\n");

    printf("  [0] 返回\n");

    printf("选择: ");

    fflush(stdout);

    int choice;

    scanf("%d", &choice);

    while (getchar() != '\n');

    switch (choice) {

        case 1:

            g_config.telegram_enabled = !g_config.telegram_enabled;

            printf("已%s\n", g_config.telegram_enabled ? "启用" : "禁用");

            break;

        case 2: {

            char input[512];

            printf("Bot Token: ");

            fgets(input, sizeof(input), stdin);

            input[strcspn(input, "\n")] = '\0';

            strncpy(g_config.telegram_token, input,

                    sizeof(g_config.telegram_token) - 1);

            printf("Chat ID: ");

            fgets(input, sizeof(input), stdin);

            input[strcspn(input, "\n")] = '\0';

            strncpy(g_config.telegram_chat_id, input,

                    sizeof(g_config.telegram_chat_id) - 1);

            printf("推送间隔(分钟): ");

            fgets(input, sizeof(input), stdin);

            g_config.telegram_interval = atoi(input);

            printf("配置已完成\n");

            break;

        }
