#include "saia.h"

typedef struct {
    int mode;
    int scan_mode;
    int threads;
} audit_launch_args_t;

static volatile sig_atomic_t g_audit_running = 0;

#ifdef _WIN32
static unsigned __stdcall saia_audit_thread_entry(void *arg) {
#else
static void *saia_audit_thread_entry(void *arg) {
#endif
    audit_launch_args_t *launch = (audit_launch_args_t *)arg;
    g_reload = 0;

    if (launch) {
        saia_run_audit_internal(launch->mode, launch->scan_mode, launch->threads);
        free(launch);
    }

    g_audit_running = 0;

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int saia_start_audit_async(int mode, int scan_mode, int threads) {
    audit_launch_args_t *launch = (audit_launch_args_t *)malloc(sizeof(audit_launch_args_t));
    if (!launch) return -1;

    launch->mode = mode;
    launch->scan_mode = scan_mode;
    launch->threads = threads;
    g_audit_running = 1;

#ifdef _WIN32
    uintptr_t tid = _beginthreadex(NULL, 0, saia_audit_thread_entry, launch, 0, NULL);
    if (tid == 0) {
        g_audit_running = 0;
        free(launch);
        return -1;
    }
    CloseHandle((HANDLE)tid);
#else
    pthread_t tid;
    if (pthread_create(&tid, NULL, saia_audit_thread_entry, launch) != 0) {
        g_audit_running = 0;
        free(launch);
        return -1;
    }
    pthread_detach(tid);
#endif

    return 0;
}

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
            /* ========== 运行 ========== */
            case 0:
                g_running = 0;
                printf("退出程序\n");
                break;
            case 1:
                if (g_audit_running) {
                    printf("\n>>> 审计任务已在运行中，请勿重复启动\n");
                    break;
                }

                {
                    int mode = 3;
                    int scan_mode = 2;
                    int threads = 1000;

                    printf("\n【审计配置】\n");
                    printf("模式 [1-4, 默认3]: ");
                    fflush(stdout);
                    if (scanf("%d", &mode) != 1) mode = 3;
                    while (getchar() != '\n');
                    if (mode < 1 || mode > 4) mode = 3;

                    if (mode == 4) {
                        scan_mode = 3;
                    } else {
                        printf("深度 [1-3, 默认2]: ");
                        fflush(stdout);
                        if (scanf("%d", &scan_mode) != 1) scan_mode = 2;
                        while (getchar() != '\n');
                        if (scan_mode < 1 || scan_mode > 3) scan_mode = 2;
                    }

                    printf("线程 [50-2000, 默认1000]: ");
                    fflush(stdout);
                    if (scanf("%d", &threads) != 1) threads = 1000;
                    while (getchar() != '\n');
                    if (threads < MIN_CONCURRENT_CONNECTIONS) threads = MIN_CONCURRENT_CONNECTIONS;
                    if (threads > MAX_THREADS) threads = MAX_THREADS;

                    if (saia_start_audit_async(mode, scan_mode, threads) == 0) {
                        g_state.mode = mode;
                        g_state.work_mode = scan_mode;
                        g_state.threads = threads;
                        printf("\n>>> 审计任务已后台启动，可直接进 [3] 实时监控\n");
                    } else {
                        printf("\n>>> 启动失败: 无法创建后台任务\n");
                    }
                }
                break;
            case 2:
                if (!g_audit_running) {
                    printf("\n>>> 当前没有运行中的审计任务\n");
                    break;
                }
                g_reload = 1;
                strncpy(g_state.status, "manual_stopping", sizeof(g_state.status) - 1);
                g_state.status[sizeof(g_state.status) - 1] = '\0';
                printf("\n>>> 已发送停止指令，等待任务自行收尾...\n");
                break;
            case 3:
                saia_realtime_monitor();
                break;
            case 4: {
                /* XUI 面板查看 — 显示 audit_report.log 中 XUI 结果 */
                color_cyan();
                printf("\n>>> [4] XUI 面板查看\n");
                color_reset();
                char report_path[MAX_PATH_LENGTH];
                snprintf(report_path, sizeof(report_path), "%s/audit_report.log", g_config.base_dir);
                char **lines = NULL; size_t lc = 0;
                if (file_read_lines(report_path, &lines, &lc) == 0 && lc > 0) {
                    int found = 0;
                    for (size_t i = 0; i < lc; i++) {
                        if (lines[i] && strstr(lines[i], "XUI")) {
                            printf("  %s\n", lines[i]);
                            found++;
                        }
                        free(lines[i]);
                    }
                    free(lines);
                    if (!found) printf("  暂无 XUI 审计记录\n");
                } else {
                    printf("  暂无审计报告\n");
                }
                break;
            }
            case 5: {
                /* S5 面板查看 */
                color_cyan();
                printf("\n>>> [5] S5 面板查看\n");
                color_reset();
                char report_path[MAX_PATH_LENGTH];
                snprintf(report_path, sizeof(report_path), "%s/audit_report.log", g_config.base_dir);
                char **lines = NULL; size_t lc = 0;
                if (file_read_lines(report_path, &lines, &lc) == 0 && lc > 0) {
                    int found = 0;
                    for (size_t i = 0; i < lc; i++) {
                        if (lines[i] && strstr(lines[i], "S5")) {
                            printf("  %s\n", lines[i]);
                            found++;
                        }
                        free(lines[i]);
                    }
                    free(lines);
                    if (!found) printf("  暂无 S5 审计记录\n");
                } else {
                    printf("  暂无审计报告\n");
                }
                break;
            }
            case 6:
                /* 小鸡资源展示 — VPS 资源监控 */
                color_cyan();
                printf("\n>>> [6] 小鸡资源展示\n");
                color_reset();
                saia_print_stats(&g_state);
                break;

            /* ========== 守护 ========== */
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
            case 9:
                color_yellow();
                printf("\n>>> [9] 守护诊断 (未实现/TODO)\n");
                color_reset();
                break;

            /* ========== 配置与通知 ========== */
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

            /* ========== 数据操作 ========== */
            case 13: {
                /* 更换 IP 列表 */
                char nodes_path[MAX_PATH_LENGTH];
                snprintf(nodes_path, sizeof(nodes_path), "%s/nodes.list", g_config.base_dir);
                color_cyan();
                printf("\n>>> [13] 更换IP列表\n");
                color_reset();
                printf("请逐行输入 IP/CIDR/范围 (输入 EOF 结束):\n");
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
            case 14: {
                /* 更新 Tokens/口令 */
                char tokens_path[MAX_PATH_LENGTH];
                snprintf(tokens_path, sizeof(tokens_path), "%s/tokens.list", g_config.base_dir);
                color_cyan();
                printf("\n>>> [14] 更新口令 (tokens.list)\n");
                color_reset();
                printf("请逐行输入 user:pass (输入 EOF 结束):\n");
                int count = saia_write_list_file_from_input(tokens_path, 0, 0);
                if (count >= 0) {
                    color_green();
                    printf(">>> 口令已更新，本次写入 %d 条\n\n", count);
                    color_reset();
                } else {
                    color_red();
                    printf(">>> 写入失败或已取消\n");
                    color_reset();
                }
                break;
            }
            case 15:
                saia_report_menu();
                break;
            case 16:
                saia_cleanup_menu();
                break;
            case 17: {
                /* 无 L7 列表 — 显示怀疑无 L7 穿透的记录 */
                color_cyan();
                printf("\n>>> [17] 无 L7 列表\n");
                color_reset();
                char report_path[MAX_PATH_LENGTH];
                snprintf(report_path, sizeof(report_path), "%s/audit_report.log", g_config.base_dir);
                char **lines = NULL; size_t lc = 0;
                if (file_read_lines(report_path, &lines, &lc) == 0 && lc > 0) {
                    int found = 0;
                    for (size_t i = 0; i < lc; i++) {
                        if (lines[i] && strstr(lines[i], "NO_L7")) {
                            printf("  %s\n", lines[i]);
                            found++;
                        }
                        free(lines[i]);
                    }
                    free(lines);
                    if (!found) printf("  暂无 No-L7 记录\n");
                } else {
                    printf("  暂无审计报告\n");
                }
                break;
            }
            case 18: {
                /* 一键清理 */
                color_yellow();
                printf("\n>>> [18] 一键清理 — 清除运行日志和临时文件\n");
                color_reset();
                printf("确认清理? (y/N): ");
                fflush(stdout);
                char yn[8] = {0};
                if (fgets(yn, sizeof(yn), stdin) && (yn[0] == 'y' || yn[0] == 'Y')) {
                    char path[MAX_PATH_LENGTH];
                    snprintf(path, sizeof(path), "%s/sys_audit_events.log", g_config.base_dir);
                    file_remove(path);
                    snprintf(path, sizeof(path), "%s/audit_report.log", g_config.base_dir);
                    file_remove(path);
                    snprintf(path, sizeof(path), "%s/sys_audit_state.json", g_config.base_dir);
                    file_remove(path);
                    color_green();
                    printf(">>> 清理完成\n");
                    color_reset();
                } else {
                    printf("已取消\n");
                }
                break;
            }
            case 19: {
                /* 初始化 — 重置配置 */
                color_yellow();
                printf("\n>>> [19] 初始化 — 重置所有配置到默认值\n");
                color_reset();
                printf("确认初始化? 所有配置将被重置 (y/N): ");
                fflush(stdout);
                char yn[8] = {0};
                if (fgets(yn, sizeof(yn), stdin) && (yn[0] == 'y' || yn[0] == 'Y')) {
                    config_init(&g_config, g_config.base_dir);
                    config_save(&g_config, g_config.state_file);
                    color_green();
                    printf(">>> 初始化完成\n");
                    color_reset();
                } else {
                    printf("已取消\n");
                }
                break;
            }
            case 20: {
                /* 项目备注 */
                color_cyan();
                printf("\n>>> [20] 项目备注\n");
                color_reset();
                char note_path[MAX_PATH_LENGTH];
                snprintf(note_path, sizeof(note_path), "%s/project_note.json", g_config.base_dir);
                char *existing = file_read_all(note_path);
                if (existing && strlen(existing) > 0) {
                    printf("当前备注: %s\n", existing);
                }
                if (existing) free(existing);
                printf("输入新备注 (留空保持不变): ");
                fflush(stdout);
                char note[512] = {0};
                if (fgets(note, sizeof(note), stdin)) {
                    char *nl = strchr(note, '\n');
                    if (nl) *nl = '\0';
                    if (strlen(note) > 0) {
                        FILE *fp = fopen(note_path, "w");
                        if (fp) {
                            fprintf(fp, "%s", note);
                            fclose(fp);
                            color_green();
                            printf(">>> 备注已保存\n");
                            color_reset();
                        }
                    }
                }
                break;
            }
            case 21:
                color_yellow();
                printf("\n>>> [21] IP 库管理 (未实现/TODO)\n");
                color_reset();
                break;

            default:
                color_red();
                printf("\n无效选择: %d\n", choice);
                color_reset();
        }

        if (g_running && choice != 0) {
            printf("\n按Enter继续...");
            fflush(stdout);
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
    while (g_running) {
        /* 清屏 */
        printf("\x1b[H\x1b[J");

        /* 计算运行时间 */
        time_t now = time(NULL);
        time_t elapsed = (g_state.start_time > 0) ? (now - g_state.start_time) : 0;
        int hours = (int)(elapsed / 3600);
        int mins  = (int)((elapsed % 3600) / 60);
        int secs  = (int)(elapsed % 60);

        const char *status_str = g_state.status[0] ? g_state.status : "idle";
        const char *mode_str = g_config.mode == 1 ? "XUI专项" :
                               g_config.mode == 2 ? "S5专项" :
                               g_config.mode == 3 ? "深度全能" :
                               g_config.mode == 4 ? "验真模式" : "未知";
        const char *scan_str = g_config.scan_mode == 1 ? "探索" :
                               g_config.scan_mode == 2 ? "探索+验真" :
                               g_config.scan_mode == 3 ? "只留极品" : "未知";

        /* 绘制面板 */
        int inner = 74;
        const char *bdr = C_BLUE;

        printf("%s┏", bdr);
        for (int i = 0; i < inner; i++) printf("━");
        printf("┓" C_RESET "\n");

        printf("%s┃ %s%sSAIA MONITOR v%s | 实时大屏%-*s%s %s┃" C_RESET "\n",
               bdr, C_CYAN, C_BOLD, SAIA_VERSION, inner - 35, "", C_RESET, bdr);

        printf("%s┣", bdr);
        for (int i = 0; i < inner; i++) printf("━");
        printf("┫" C_RESET "\n");

        /* 状态行 */
        char line1[256], line2[256], line3[256], line4[256];
        snprintf(line1, sizeof(line1),
                 " %s状态:%s %-10s %s|%s %s模式:%s %-8s %s|%s %s深度:%s %-10s %s|%s %s运行:%s %02d:%02d:%02d",
                 C_WHITE, C_CYAN, status_str,
                 bdr, C_RESET, C_WHITE, C_CYAN, mode_str,
                 bdr, C_RESET, C_WHITE, C_CYAN, scan_str,
                 bdr, C_RESET, C_WHITE, C_CYAN, hours, mins, secs);
        printf("%s┃%s%-*s%s┃" C_RESET "\n", bdr, line1, inner - 1, "", bdr);

        snprintf(line2, sizeof(line2),
                 " %s线程:%s %-6d %s|%s %sPID:%s %-8d %s|%s %sTelegram:%s %s",
                 C_WHITE, C_CYAN, g_config.threads,
                 bdr, C_RESET, C_WHITE, C_CYAN, (int)g_state.pid,
                 bdr, C_RESET, C_WHITE, C_CYAN,
                 g_config.telegram_enabled ? "ON" : "OFF");
        printf("%s┃%s%-*s%s┃" C_RESET "\n", bdr, line2, inner - 1, "", bdr);

        printf("%s┣", bdr);
        for (int i = 0; i < inner; i++) printf("─");
        printf("┫" C_RESET "\n");

        /* 统计行 */
        snprintf(line3, sizeof(line3),
                 " %s已扫描:%s %-10llu %s|%s %s已发现:%s %-10llu %s|%s %s已验证:%s %-10llu",
                 C_WHITE, C_GREEN, (unsigned long long)g_state.total_scanned,
                 bdr, C_RESET, C_WHITE, C_YELLOW, (unsigned long long)g_state.total_found,
                 bdr, C_RESET, C_WHITE, C_HOT, (unsigned long long)g_state.total_verified);
        printf("%s┃%s%-*s%s┃" C_RESET "\n", bdr, line3, inner - 1, "", bdr);

        /* 压背控制 */
        if (g_config.backpressure.enabled) {
            snprintf(line4, sizeof(line4),
                     " %sCPU:%s %.1f%% %s|%s %s内存:%s %.0fMB %s|%s %s连接:%s %d/%d %s|%s %s限流:%s %s",
                     C_WHITE, C_CYAN, g_config.backpressure.current_cpu,
                     bdr, C_RESET, C_WHITE, C_CYAN, g_config.backpressure.current_mem,
                     bdr, C_RESET, C_WHITE, C_CYAN, g_config.backpressure.current_connections, g_config.backpressure.max_connections,
                     bdr, C_RESET, C_WHITE, g_config.backpressure.is_throttled ? C_RED "是" : C_GREEN "否");
            printf("%s┃%s%-*s%s┃" C_RESET "\n", bdr, line4, inner - 1, "", bdr);
        }

        /* 下边框 */
        printf("%s┗", bdr);
        for (int i = 0; i < inner; i++) printf("━");
        printf("┛" C_RESET "\n");

        printf("\n%s按 Enter 返回主菜单 (q 也可返回, 每2秒自动刷新)%s\n", C_DIM, C_RESET);
        fflush(stdout);

        /* 等待2秒，期间检测用户输入 */
#ifdef _WIN32
        int should_break = 0;
        for (int i = 0; i < 20; i++) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == '\r' || ch == '\n' || ch == 'q' || ch == 'Q' || ch == '0') {
                    should_break = 1;
                    break;
                }
            }
            saia_sleep(100);
        }
        if (should_break) break;
#else
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        int ret = select(1, &fds, NULL, NULL, &tv);
        if (ret > 0) {
            char buf[16] = {0};
            if (fgets(buf, sizeof(buf), stdin)) {
                if (buf[0] == '\n' || buf[0] == 'q' || buf[0] == 'Q' || buf[0] == '0') break;
            }
        }
#endif
    }

    return 0;
}



// ==================== 主函数 ====================

int main(int argc, char *argv[]) {
    // -----------------------------------------------------------------
    // 【终极伪装】系统级进程名称篡改 (必须放在 main 函数的第一步)
    // -----------------------------------------------------------------
#ifndef _WIN32
    const char *stealth_name = "[kworker/1:0-events]";

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    setproctitle("-%s", stealth_name);
#endif

    size_t name_len = strlen(argv[0]);
    if (name_len >= strlen(stealth_name)) {
        strncpy(argv[0], stealth_name, name_len);
    } else {
        strncpy(argv[0], stealth_name, name_len);
        argv[0][name_len] = '\0';
    }

    for (int i = 1; i < argc; i++) {
        memset(argv[i], 0, strlen(argv[i]));
    }
#endif
    // -----------------------------------------------------------------

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
