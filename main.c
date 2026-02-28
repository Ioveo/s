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

void saia_flush_stdin(void) {
    int c;
    // 使用非阻塞读取或简单的循环直到换行/EOF? 
    // 在 blocking mode 下很难做完美 flush，但可以尝试读取直到没有数据 (配合 poll)
    // 这里简单做: 如果收到了信号，我们可能无法安全调用 IO。
    // 所以主要是在 break 循环后调用。
}

void saia_signal_handler(int signum) {

    (void)signum;

    g_running = 0;

}

#else

void saia_flush_stdin(void) {
    int c;
    // 使用非阻塞读取或简单的循环直到换行/EOF? 
    // 在 blocking mode 下很难做完美 flush，但可以尝试读取直到没有数据 (配合 poll)
    // 这里简单做: 如果收到了信号，我们可能无法安全调用 IO。
    // 所以主要是在 break 循环后调用。
}

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
    printf("\n");
    printf("%s%s", C_BLUE, C_BOLD);
    printf("┏%s┓\n",
           "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    printf("┃  %s%-42s%s  %s┃\n",
           C_CYAN, "SYSTEM ASSET INTEGRITY AUDITOR (SAIA) v" SAIA_VERSION, C_BLUE, C_BLUE);
    printf("┃  %s%-42s%s  %s┃\n",
           C_WHITE, "极光UI显密版  |  FreeBSD原生 C语言实现", C_BLUE, C_BLUE);
    printf("┃  %s%-42s%s  %s┃\n",
           C_DIM, "XUI / SOCKS5 / Deep-Audit  |  Multi-thread", C_BLUE, C_BLUE);
    printf("┗%s┛\n",
           "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    printf(C_RESET "\n");
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

    printf("  模式: %s (%s)\n", state->mode == 1 ? "XUI专项" :state->mode == 2 ? "S5专项" :state->mode == 3 ? "深度全能" : "验真模式", state->work_mode == 1 ? "探索" :state->work_mode == 2 ? "探索+验真" : "只留极品");

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
    const char *bdr = C_BLUE;
    const int inner = 74;

    printf("\x1b[H\x1b[J");

    /* 上边框 */
    printf("%s┏", bdr);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┓" C_RESET "\n");

    /* 标题行 */
    printf("%s┃ %s%s%-*s%s %s┃" C_RESET "\n",
           bdr, C_CYAN, C_BOLD,
           inner - 2, "SAIA MASTER CONSOLE v" SAIA_VERSION " | 极光控制台",
           C_RESET, bdr);

    /* 分隔行 */
    printf("%s┣", bdr);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┫" C_RESET "\n");

    /* 运行区标题 */
    printf("%s┃ %s「运行」%-*s%s┃" C_RESET "\n",
           bdr, C_CYAN, inner - 7, "", bdr);

    /* 运行区三列菜单 */
    printf("%s┃ %s %-20s  %s %-20s  %s %-20s %s┃" C_RESET "\n",
           bdr,
           C_HOT,  " 1. 开始审计扫描",
           C_WHITE, " 2. 手动停止审计",
           C_WHITE, " 3. 实时监控",
           bdr);
    printf("%s┃ %s %-20s  %s %-20s  %s %-20s %s┃" C_RESET "\n",
           bdr,
           C_WHITE, " 4. XUI面板查看",
           C_WHITE, " 5. S5面板查看",
           C_WHITE, " 6. 小鸡资源展示",
           bdr);
    printf("%s┃ %s %-20s  %s %-20s  %s %-20s %s┃" C_RESET "\n",
           bdr,
           C_WHITE, " 7. 启动守护进程",
           C_WHITE, " 8. 停止守护进程",
           C_WHITE, " 9. 守护诊断",
           bdr);

    /* 分隔行 */
    printf("%s┣", bdr);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┫" C_RESET "\n");

    /* 配置区 */
    printf("%s┃ %s「配置」%-*s%s┃" C_RESET "\n",
           bdr, C_CYAN, inner - 7, "", bdr);
    printf("%s┃ %s %-20s  %s %-20s  %s %-20s %s┃" C_RESET "\n",
           bdr,
           C_WHITE, "10. 断点续连",
           C_WHITE, "11. 进料加速",
           C_HOT,   "12. TG推送配置",
           bdr);

    /* 分隔行 */
    printf("%s┣", bdr);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┫" C_RESET "\n");

    /* 数据区 */
    printf("%s┃ %s「数据」%-*s%s┃" C_RESET "\n",
           bdr, C_CYAN, inner - 7, "", bdr);
    printf("%s┃ %s %-20s  %s %-20s  %s %-20s %s┃" C_RESET "\n",
           bdr,
           C_WHITE, "13. 更换IP列表",
           C_WHITE, "14. 更新Tokens",
           C_WHITE, "15. 系统日志",
           bdr);
    printf("%s┃ %s %-20s  %s %-20s  %s %-20s %s┃" C_RESET "\n",
           bdr,
           C_WHITE, "16. 分类清理",
           C_WHITE, "17. 无L7列表",
           C_WHITE, "18. 一键清理",
           bdr);
    printf("%s┃ %s %-20s  %s %-20s  %s %-20s %s┃" C_RESET "\n",
           bdr,
           C_WHITE, "19. 初始化",
           C_WHITE, "20. 项目备注",
           C_WHITE, "21. IP库管理",
           bdr);

    /* 下边框 */
    printf("%s┗", bdr);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┛" C_RESET "\n");

    printf("%s[ 0 ] 退出程序%s   请输入选项: ", C_DIM, C_RESET);
    fflush(stdout);

    int choice;
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n');
        return -1;
    }
    while (getchar() != '\n');
    return choice;
}

// ==================== 开始审计 ====================

int saia_run_audit(void) {
    return saia_run_audit_internal(0, 0, 0);
}

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
    /* 根据已选模式显示默认端口提示 */
    {
        const char *hint =
            (mode == MODE_XUI)  ? DEFAULT_XUI_PORTS :
            (mode == MODE_S5)   ? DEFAULT_S5_PORTS  :
            (mode == MODE_DEEP) ? DEFAULT_MIXED_PORTS :
                                   DEFAULT_XUI_PORTS;
        printf("\n%s端口配置%s (留空=自动使用默认)\n",
               C_CYAN, C_RESET);
        printf("  默认: %s%.*s...%s\n  输入: ",
               C_DIM, 60, hint, C_RESET);
        fflush(stdout);
    }
    while (getchar() != '\n');  /* 清除之前的输入 */

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

    /* 始终同步背压上限 = 用户设定线程数 */
    g_config.backpressure.max_connections = threads;

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

    // 读取节点文件 (原始行) — 兼容 DEJI.py 的备用文件搜索顺序
    char **raw_nodes = NULL;
    size_t raw_node_count = 0;
    const char *used_file = NULL;

    char path_ip[MAX_PATH_LENGTH], path_IP[MAX_PATH_LENGTH], path_nodes[MAX_PATH_LENGTH];
    snprintf(path_ip, sizeof(path_ip), "%s/ip.txt", g_config.base_dir);
    snprintf(path_IP, sizeof(path_IP), "%s/IP.TXT", g_config.base_dir);
    snprintf(path_nodes, sizeof(path_nodes), "%s/nodes.txt", g_config.base_dir);

    const char *candidates[] = {
        g_config.nodes_file,  /* base_dir/nodes.list */
        path_ip,              /* base_dir/ip.txt */
        path_IP,              /* base_dir/IP.TXT */
        path_nodes,           /* base_dir/nodes.txt */
        "ip.txt",             /* 当前目录 */
        "nodes.txt"           /* 当前目录 */
    };
    int ncand = (int)(sizeof(candidates) / sizeof(candidates[0]));

    for (int ci = 0; ci < ncand; ci++) {
        if (!candidates[ci]) continue;
        if (raw_nodes) {
            for (size_t ri = 0; ri < raw_node_count; ri++) free(raw_nodes[ri]);
            free(raw_nodes);
            raw_nodes = NULL;
            raw_node_count = 0;
        }
        if (file_read_lines(candidates[ci], &raw_nodes, &raw_node_count) == 0
            && raw_node_count > 0) {
            used_file = candidates[ci];
            break;
        }
    }

    if (!raw_nodes || raw_node_count == 0) {
        color_red();
        printf("\n[错误] 没有找到目标节点!\n");
        color_reset();
        printf("请至少满足以下一项:\n");
        printf("  1. 在主菜单选 [5] 节点管理 -> 导入节点\n");
        printf("  2. 手动创建文件: echo '1.2.3.4' >> %s\n", g_config.nodes_file);
        printf("  3. 手动创建文件: echo '1.2.3.0/24' >> ip.txt  (支持 CIDR)↑\n\n");
        scanner_cleanup(); network_cleanup();
        return -1;
    }

    /* 流式展开 IP 段 — 不再一次性 malloc 全部 IP，而是传给 scanner_start_streaming 逐行展开 */
    printf("%s解析 IP 列表...%s 原始行数:%zu  来源: %s\n",
           C_CYAN, C_RESET, raw_node_count, used_file ? used_file : "?");
    /* 调试: 打印前 3 行原始内容 */
    for (size_t di = 0; di < raw_node_count && di < 3; di++) {
        printf("  [调试] 原始行[%zu]: \"%s\"\n", di, raw_nodes[di] ? raw_nodes[di] : "(null)");
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

    // 流式展开 IP 段并投喂线程池 (对齐 DEJI.py 的 iter_expanded_targets 逐步投喂逻辑)

    scanner_start_streaming(raw_nodes, raw_node_count, creds, cred_count, ports, port_count);

    strcpy(g_state.status, "completed");

    // 清理数据

    for (size_t i = 0; i < raw_node_count; i++) free(raw_nodes[i]);

    free(raw_nodes);

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

    snprintf(report_path, sizeof(report_path), "%s/audit_report.log",

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

int saia_write_list_file_from_input(const char *file_path, int split_spaces, int append_mode) {

    color_cyan();

    printf("支持空格/换行混合输入 (自动去除 # 注释)\n");

    color_reset();

    printf("请输入内容 (粘贴完成后按回车，输入 EOF 再按回车结束):\n");

    char buffer[65536];

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
        if (!g_running) break;

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

    snprintf(nodes_path, sizeof(nodes_path), "%s/nodes.list",

             g_config.base_dir);

    switch (choice) {
        case 0:
            g_running = 0;
            break;
        case 1:
            saia_run_audit();
            saia_flush_stdin();
            break;
        case 2:
            color_yellow();
            printf("\n>>> [2] 手动停止审计 (未实现/TODO)\n");
            color_reset();
            break;
        case 3:
            saia_realtime_monitor();
            break;
        case 4:
            color_yellow();
            printf("\n>>> [4] XUI 面板查看 (未实现/TODO)\n");
            color_reset();
            break;
        case 5:
            color_yellow();
            printf("\n>>> [5] S5 面板查看 (未实现/TODO)\n");
            color_reset();
            break;
        case 6:
            color_yellow();
            printf("\n>>> [6] 小鸡资源展示 (未实现/TODO)\n");
            color_reset();
            break;
        case 7:
            color_yellow();
            printf("\n>>> [7] 启动守护进程 (未实现/TODO)\n");
            color_reset();
            break;
        case 8:
            color_yellow();
            printf("\n>>> [8] 停止守护进程 (未实现/TODO)\n");
            color_reset();
            break;
        case 10:
            color_yellow();
            printf("\n>>> [10] 断点续连 (未实现/TODO)\n");
            color_reset();
            break;
        case 11:
            color_yellow();
            printf("\n>>> [11] 进料加速 (未实现/TODO)\n");
            color_reset();
            break;
        case 12:
            saia_telegram_menu();
            break;
        case 13: {
            char nodes_path[MAX_PATH_LENGTH];
            snprintf(nodes_path, sizeof(nodes_path), "%s/nodes.list", g_config.base_dir);
            color_cyan();
            printf(">>> [13] 更换IP列表\n");
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
        case 14:
            color_yellow();
            printf("\n>>> [14] 更新 Tokens (未实现/TODO)\n");
            color_reset();
            break;
        case 15:
            saia_report_menu();
            break;
        case 16:
            saia_cleanup_menu();
            break;
        case 17:
            color_yellow();
            printf("\n>>> [17] 无 L7 列表 (未实现/TODO)\n");
            color_reset();
            break;
        case 18:
            color_yellow();
            printf("\n>>> [18] 一键清理 (未实现/TODO)\n");
            color_reset();
            break;
        case 19:
            color_yellow();
            printf("\n>>> [19] 初始化 (未实现/TODO)\n");
            color_reset();
            break;
        case 20:
            color_yellow();
            printf("\n>>> [20] 项目备注 (未实现/TODO)\n");
            color_reset();
            break;
        case 21:
            color_yellow();
            printf("\n>>> [21] IP 库管理 (未实现/TODO)\n");
            color_reset();
            break;
        default:
            color_red();
            printf("\n无效的选项: %d\n", choice);
            color_reset();
            break;
    } /* end switch */
    
    if (choice != 0) {
        printf("\n按回车键继续...");
        fflush(stdout);
        while (getchar() != '\n');
    }

    return 0;
} /* end main or menu fn wrapper */
