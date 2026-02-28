#include "saia.h"

int saia_backpressure_menu(void) {
    color_yellow();
    printf("\n【压背控制】\n");
    color_reset();

    printf("当前状态: %s\n\n", g_config.backpressure.enabled ? "已启用" : "已禁用");

    printf("配置:\n");
    printf("  CPU阈值: %.1f%%\n", g_config.backpressure.cpu_threshold);
    printf("  内存阈值: %.1f MB\n", g_config.backpressure.mem_threshold);
    printf("  最大连接: %d\n", g_config.backpressure.max_connections);
    printf("  实时CPU: %.1f%%\n", g_config.backpressure.current_cpu);
    printf("  实时内存: %.1f MB\n", g_config.backpressure.current_mem);
    printf("  当前连接: %d\n", g_config.backpressure.current_connections);
    printf("  限流状态: %s\n", g_config.backpressure.is_throttled ? "是" : "否");

    printf("\n  [1] 启用/禁用\n");
    printf("  [2] 调整阈值\n");
    printf("  [3] 立即检查\n");
    printf("  [0] 返回\n");

    printf("选择: ");
    fflush(stdout);

    int choice;
    scanf("%d", &choice);
    while (getchar() != '\n');

    switch (choice) {
        case 1:
            g_config.backpressure.enabled = !g_config.backpressure.enabled;
            printf("已%s压背控制\n", g_config.backpressure.enabled ? "启用" : "禁用");
            if (g_config.backpressure.enabled) {
                backpressure_init(&g_config.backpressure);
            }
            break;

        case 2: {
            char input[256];
            printf("CPU阈值 [%%] [当前=%.1f]: ",
                   g_config.backpressure.cpu_threshold);
            fgets(input, sizeof(input), stdin);
            if (strlen(input) > 1) {
                g_config.backpressure.cpu_threshold = atof(input);
            }

            printf("内存阈值 [MB] [当前=%.1f]: ",
                   g_config.backpressure.mem_threshold);
            fgets(input, sizeof(input), stdin);
            if (strlen(input) > 1) {
                g_config.backpressure.mem_threshold = atof(input);
            }

            printf("最大连接数 [当前=%d]: ",
                   g_config.backpressure.max_connections);
            fgets(input, sizeof(input), stdin);
            if (strlen(input) > 1) {
                g_config.backpressure.max_connections = atoi(input);
            }

            printf("配置已更新\n");
            break;
        }

        case 3:
            backpressure_update(&g_config.backpressure);
            printf("检查结果:\n");
            printf("  CPU: %.1f%% %s %.1f%%\n",
                   g_config.backpressure.current_cpu,
                   g_config.backpressure.current_cpu > g_config.backpressure.cpu_threshold ? ">" : "<=",
                   g_config.backpressure.cpu_threshold);
            printf("  内存: %.1f MB %s %.1f MB\n",
                   g_config.backpressure.current_mem,
                   g_config.backpressure.current_mem > g_config.backpressure.mem_threshold ? ">" : "<=",
                   g_config.backpressure.mem_threshold);
            printf("  建议: %s\n",
                   backpressure_should_throttle(&g_config.backpressure) ?
                   "应该限流" : "正常运行");
            break;

        default:
            return 0;
    }

    return 0;
}

// ==================== 清理数据 ====================

int saia_cleanup_menu(void) {
    color_yellow();
    printf("\n【清理数据】\n");
    color_reset();

    printf("警告: 此操作将清除所有运行时数据!\n");
    printf("  [1] 清理日志\n");
    printf("  [2] 清理报告\n");
    printf("  [3] 清理状态\n");
    printf("  [4] 全部清理\n");
    printf("  [0] 取消\n");

    printf("选择: ");
    fflush(stdout);

    int choice;
    scanf("%d", &choice);
    while (getchar() != '\n');

    const char *base = g_config.base_dir;
    char path[MAX_PATH_LENGTH];
    int removed = 0;

    switch (choice) {
        case 1:
            snprintf(path, sizeof(path), "%s/sys_audit_events.log", base);
            if (file_remove(path) == 0) { removed++; }
            printf("已清理 %d 个日志文件\n", removed);
            break;

        case 2:
            snprintf(path, sizeof(path), "%s/audit_report.log", base);
            if (file_remove(path) == 0) { removed++; }
            printf("已清理 %d 个报告文件\n", removed);
            break;

        case 3:
            snprintf(path, sizeof(path), "%s/sys_audit_state.json", base);
            if (file_remove(path) == 0) { removed++; }
            printf("已清理 %d 个状态文件\n", removed);
            break;

        case 4:
            snprintf(path, sizeof(path), "%s/sys_audit_events.log", base);
            if (file_remove(path) == 0) { removed++; }
            snprintf(path, sizeof(path), "%s/audit_report.log", base);
            if (file_remove(path) == 0) { removed++; }
            snprintf(path, sizeof(path), "%s/sys_audit_state.json", base);
            if (file_remove(path) == 0) { removed++; }
            snprintf(path, sizeof(path), "%s/audit_runner.lock", base);
            if (file_remove(path) == 0) { removed++; }
            printf("已清理 %d 个文件\n", removed);
            break;

        default:
            return 0;
    }

    return 0;
}

// ==================== 主循环 ====================

int saia_interactive_mode(void) {
    while (g_running) {
        int choice = saia_print_menu();

        switch (choice) {
            case 1:
                saia_run_audit_internal(0, 0, 0);
                break;

            case 2:
                saia_config_menu();
                break;

            case 3:
                saia_realtime_monitor();
                break;

            /* case 10: 报表查看 (临时用子菜单内维持) */

            case 4:
                printf("功能开发中...\n");
                break;

            case 5:
                saia_nodes_menu();
                break;

            case 6:
                saia_credentials_menu();
                break;

            case 7:
                saia_telegram_menu();
                break;

            case 8:
                saia_backpressure_menu();
                break;

            case 9:
                saia_cleanup_menu();
                break;

            case 0:
                g_running = 0;
                printf("退出程序\n");
                break;

            default:
                color_red();
                printf("无效选择\n");
                color_reset();
        }

        if (g_running) {
            printf("\n按Enter继续...");
            getchar();
        }
    }

    return 0;
}

// ==================== Telegram 推送菜单 ====================

int saia_telegram_menu(void) {
    color_yellow();
    printf("\n【Telegram 推送】\n");
    color_reset();

    printf("当前状态: %s\n", g_config.telegram_enabled ? "已启用" : "已禁用");
    if (g_config.telegram_enabled) {
        printf("  Bot Token: %s***\n", g_config.telegram_token);
        printf("  Chat ID:   %s\n", g_config.telegram_chat_id);
        printf("  推送间隔:  %d 分钟\n", g_config.telegram_interval);
    }

    printf("\n  [1] 启用/禁用\n");
    printf("  [2] 配置 Bot Token\n");
    printf("  [3] 配置 Chat ID\n");
    printf("  [4] 配置推送间隔 (分钟)\n");
    printf("  [5] 发送测试消息\n");
    printf("  [0] 返回\n");
    printf("选择: ");
    fflush(stdout);

    int choice;
    if (scanf("%d", &choice) != 1) choice = 0;
    while (getchar() != '\n');

    char input[512];
    switch (choice) {
        case 1:
            g_config.telegram_enabled = !g_config.telegram_enabled;
            printf("Telegram 推送已%s\n", g_config.telegram_enabled ? "启用" : "禁用");
            break;
        case 2:
            printf("Bot Token: ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = '\0';
                strncpy(g_config.telegram_token, input, sizeof(g_config.telegram_token) - 1);
                printf("已更新\n");
            }
            break;
        case 3:
            printf("Chat ID: ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\n")] = '\0';
                strncpy(g_config.telegram_chat_id, input, sizeof(g_config.telegram_chat_id) - 1);
                printf("已更新\n");
            }
            break;
        case 4:
            printf("推送间隔 (分钟, 0=禁用): ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin)) {
                g_config.telegram_interval = atoi(input);
                printf("已更新为 %d 分钟\n", g_config.telegram_interval);
            }
            break;
        case 5: {
            if (!g_config.telegram_enabled) {
                printf("请先启用 TG 推送。\n");
                break;
            }
            printf("正在发送测试消息...\n");
            int ret = push_telegram("<b>SAIA 通知</b>\n\nTG 推送测试成功，当前版本 " SAIA_VERSION " 运行正常。");
            if (ret == 0) printf("消息发送请求已执行(请查看是否收到)。\n");
            else printf("消息发送请求下发失败。\n");
            break;
        }
        default:
            break;
    }
    return 0;
}

// ==================== 凭据管理菜单 ====================

int saia_credentials_menu(void) {
    color_yellow();
    printf("\n【凭据管理】\n");
    color_reset();

    printf("凭据文件: %s\n", g_config.tokens_file);

    char **lines = NULL;
    size_t count = 0;
    if (file_read_lines(g_config.tokens_file, &lines, &count) == 0 && count > 0) {
        printf("当前条数: %zu\n", count);
        for (size_t i = 0; i < count; i++) free(lines[i]);
        free(lines);
    } else {
        printf("当前条数: 0 (文件不存在或为空)\n");
    }

    printf("\n  [1] 替换凭据列表\n");
    printf("  [2] 追加凭据\n");
    printf("  [0] 返回\n");
    printf("选择: ");
    fflush(stdout);

    int choice;
    if (scanf("%d", &choice) != 1) choice = 0;
    while (getchar() != '\n');

    switch (choice) {
        case 1: {
            color_cyan();
            printf(">>> 替换凭据列表 (格式: user:pass 或 pass)\n");
            color_reset();
            int n = saia_write_list_file_from_input(g_config.tokens_file, 0, 0);
            if (n >= 0) {
                color_green();
                printf(">>> 已写入 %d 条凭据\n", n);
                color_reset();
            }
            break;
        }
        case 2: {
            color_cyan();
            printf(">>> 追加凭据 (格式: user:pass 或 pass)\n");
            color_reset();
            int n = saia_write_list_file_from_input(g_config.tokens_file, 0, 1);
            if (n >= 0) {
                color_green();
                printf(">>> 已追加 %d 条凭据\n", n);
                color_reset();
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

// ==================== 大屏面板工具函数 (对应 DEJI.py 极光UI) ====================

/* 计算包含 ANSI 转义的字符串实际显示宽度（中文算 2） */
static int visible_width(const char *s) {
    int w = 0;
    int in_esc = 0;
    while (s && *s) {
        if (in_esc) {
            if (*s == 'm') in_esc = 0;
            s++;
            continue;
        }
        if (*s == '\033') { in_esc = 1; s++; continue; }
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) { w += 1; s++; }
        else if ((c & 0xE0) == 0xC0) {
            /* UTF-8 2 byte */
            unsigned int cp = (c & 0x1F) << 6;
            if (*(s+1)) cp |= (*(s+1) & 0x3F);
            w += (cp >= 0xFF01 && cp <= 0xFF60) ? 2 : 1;
            s += 2;
        } else if ((c & 0xF0) == 0xE0) {
            /* UTF-8 3 byte (CJK 在此范围) */
            w += 2;
            s += 3;
        } else if ((c & 0xF8) == 0xF0) {
            /* UTF-8 4 byte (emoji 等) */
            w += 2;
            s += 4;
        } else { w += 1; s++; }
    }
    return w;
}

/* 单行带边框输出，自动补齐 */
static void render_panel_line(const char *text, int inner) {
    int w = visible_width(text);
    int pad = inner - 1 - w;
    if (pad < 0) pad = 0;
    printf("%s┃ %s%*s%s┃%s\n", C_BLUE, text, pad, "", C_BLUE, C_RESET);
}

/* 单板构建 */
static void render_single_panel(const char *title, const char **lines, int nlines, int inner, int rows) {
    /* 上边框 */
    printf("%s┏", C_BLUE);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┓%s\n", C_RESET);
    /* 标题 */
    render_panel_line(title, inner);
    /* 分隔 */
    printf("%s┣", C_BLUE);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┫%s\n", C_RESET);
    /* 内容行 */
    int shown = 0;
    for (int i = 0; i < nlines && shown < rows; i++, shown++)
        render_panel_line(lines[i], inner);
    while (shown < rows) {
        render_panel_line("", inner);
        shown++;
    }
    /* 下边框 */
    printf("%s┗", C_BLUE);
    for (int i = 0; i < inner; i++) printf("━");
    printf("┛%s\n", C_RESET);
}

/* 实时监控: 读取 sys_audit_state.json 并显示双栏大屏 */
int saia_realtime_monitor(void) {
    printf("%s\u5f53前功能开发中，临时显示状态文件内容\u2026%s\n", C_YELLOW, C_RESET);

    /* 读取 JSON 状态文件 */
    size_t fsz = 0;
    char *raw = file_read_all_n(g_config.state_file, &fsz);
    if (!raw) {
        printf("%s审计进程未运行（%s 不存在）%s\n",
               C_DIM, g_config.state_file, C_RESET);
        return 0;
    }

    /* 简单显示状态文件内容 */
    const char *left_title = C_CYAN "SAIA MONITOR | " C_WHITE "实时状态";
    const char *lines[16];
    char line_buf[16][128];
    int nl = 0;

    /* 提取几个关键字段并显示 */
    const char *fields[] = {"status", "mode", "done", "total",
                             "xui_found", "xui_verified", "s5_found", "s5_verified",
                             "pid", "threads", "current", NULL};
    for (int i = 0; fields[i] && nl < 11; i++) {
        /* 简单 JSON string-search，足够应对状态文件 */
        char pat[64];
        snprintf(pat, sizeof(pat), "\"%s\"", fields[i]);
        char *p = strstr(raw, pat);
        if (!p) continue;
        p += strlen(pat);
        while (*p == ' ' || *p == ':') p++;
        /* 取值 */
        char val[96] = "-";
        if (*p == '"') {
            p++;
            int vi = 0;
            while (*p && *p != '"' && vi < 90) val[vi++] = *p++;
            val[vi] = 0;
        } else {
            int vi = 0;
            while (*p && *p != ',' && *p != '}' && *p != '\n' && vi < 90)
                val[vi++] = *p++;
            val[vi] = 0;
        }
        snprintf(line_buf[nl], sizeof(line_buf[nl]),
                 "%s%-12s%s: %s%s%s",
                 C_WHITE, fields[i], C_RESET, C_CYAN, val, C_RESET);
        lines[nl] = line_buf[nl];
        nl++;
    }
    free(raw);

    render_single_panel(left_title, lines, nl, 72, 12);
    printf("\n%s尚未支持自动刷新。%s按 Enter 返回主菜单…", C_DIM, C_RESET);
    fflush(stdout);
    while (getchar() != '\n');
    return 0;
}

// ==================== 主函数 ====================

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#ifdef _WIN32
    SetConsoleCtrlHandler(saia_console_handler, TRUE);
    SetConsoleOutputCP(65001); /* UTF-8 */
#else
    signal(SIGINT,  saia_signal_handler);
    signal(SIGTERM, saia_signal_handler);
    signal(SIGHUP,  saia_signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    /* 初始化配置 */
    if (config_init(&g_config, getenv("HOME") ? getenv("HOME") : ".") != 0) {
        fprintf(stderr, "配置初始化失败\n");
        return 1;
    }

    saia_print_banner();

    /* 进入交互式主循环 */
    saia_interactive_mode();

    saia_cleanup();
    return 0;
}
