#include "saia.h"

typedef struct {
    int mode;
    int scan_mode;
    int threads;
    int port_batch_size;
} audit_launch_args_t;

static volatile sig_atomic_t g_audit_running = 0;

static const char *saia_dash_spinner(int running) {
    static const char *frames[] = {"[•□□□]", "[□•□□]", "[□□•□]", "[□□□•]"};
    if (!running) return "[□□□□]";
    time_t now = time(NULL);
    return frames[(int)(now % 4)];
}

static int saia_is_scan_session_running(void) {
#ifdef _WIN32
    return g_audit_running ? 1 : 0;
#else
    FILE *pp = popen("screen -list 2>/dev/null", "r");
    if (!pp) return 0;
    char line[512];
    int running = 0;
    while (fgets(line, sizeof(line), pp)) {
        if (strstr(line, "saia_scan")) {
            running = 1;
            break;
        }
    }
    pclose(pp);
    return running;
#endif
}

static int saia_count_report_stats(const char *report_path,
                                   uint64_t *xui_found,
                                   uint64_t *xui_verified,
                                   uint64_t *s5_found,
                                   uint64_t *s5_verified,
                                   uint64_t *total_found,
                                   uint64_t *total_verified) {
    if (xui_found) *xui_found = 0;
    if (xui_verified) *xui_verified = 0;
    if (s5_found) *s5_found = 0;
    if (s5_verified) *s5_verified = 0;
    if (total_found) *total_found = 0;
    if (total_verified) *total_verified = 0;

    char **lines = NULL;
    size_t lc = 0;
    if (file_read_lines(report_path, &lines, &lc) != 0 || !lines) return -1;

    for (size_t i = 0; i < lc; i++) {
        const char *s = lines[i] ? lines[i] : "";
        if (strstr(s, "[XUI_FOUND]")) {
            if (xui_found) (*xui_found)++;
            if (total_found) (*total_found)++;
        }
        if (strstr(s, "[S5_FOUND]")) {
            if (s5_found) (*s5_found)++;
            if (total_found) (*total_found)++;
        }
        if (strstr(s, "[XUI_VERIFIED]")) {
            if (xui_verified) (*xui_verified)++;
            if (total_verified) (*total_verified)++;
        }
        if (strstr(s, "[S5_VERIFIED]")) {
            if (s5_verified) (*s5_verified)++;
            if (total_verified) (*total_verified)++;
        }
        free(lines[i]);
    }
    free(lines);
    return 0;
}

#ifdef _WIN32
static unsigned __stdcall saia_audit_thread_entry(void *arg) {
#else
static void *saia_audit_thread_entry(void *arg) {
#endif
    audit_launch_args_t *launch = (audit_launch_args_t *)arg;
    g_reload = 0;

    if (launch) {
        saia_run_audit_internal(launch->mode, launch->scan_mode, launch->threads, launch->port_batch_size);
        free(launch);
    }

    g_audit_running = 0;

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int saia_start_audit_async(int mode, int scan_mode, int threads, int port_batch_size) {
    audit_launch_args_t *launch = (audit_launch_args_t *)malloc(sizeof(audit_launch_args_t));
    if (!launch) return -1;

    launch->mode = mode;
    launch->scan_mode = scan_mode;
    launch->threads = threads;
    launch->port_batch_size = port_batch_size;
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
        if (choice == -2) {
            continue;
        }

        switch (choice) {
            /* ========== 运行 ========== */
            case 0:
                g_running = 0;
                printf("退出程序\n");
                break;
            case 1:
                if (saia_is_scan_session_running()) {
                    printf("\n>>> 审计任务已在运行，请先停止或等待完成\n");
                    break;
                }

                int port_batch_size = 5;
                {
                    char input[256] = {0};

                    color_cyan();
                    printf("\n>>> 启动前配置 (留空使用当前值)\n");
                    color_reset();

                    printf("模式 [1-4] (当前 %d): ", g_config.mode);
                    fflush(stdout);
                    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
                        int mode = atoi(input);
                        if (mode >= 1 && mode <= 4) g_config.mode = mode;
                    }

                    printf("扫描策略 [1-3] (当前 %d): ", g_config.scan_mode);
                    fflush(stdout);
                    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
                        int scan_mode = atoi(input);
                        if (scan_mode >= 1 && scan_mode <= 3) g_config.scan_mode = scan_mode;
                    }

                    printf("并发线程 [50-300] (当前 %d): ", g_config.threads);
                    fflush(stdout);
                    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
                        int threads = atoi(input);
                        if (threads >= MIN_CONCURRENT_CONNECTIONS && threads <= 300) {
                            g_config.threads = threads;
                        }
                    }

                    printf("端口分批大小 [1-30] (默认5): ");
                    fflush(stdout);
                    if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
                        int batch = atoi(input);
                        if (batch >= 1 && batch <= 30) {
                            port_batch_size = batch;
                        }
                    }

                    if (g_config.threads < MIN_CONCURRENT_CONNECTIONS) g_config.threads = MIN_CONCURRENT_CONNECTIONS;
                    if (g_config.threads > 300) g_config.threads = 300;

                    config_save(&g_config, g_config.state_file);
                }

#ifdef _WIN32
                if (saia_start_audit_async(g_config.mode, g_config.scan_mode, g_config.threads, port_batch_size) != 0) {
                    printf("\n>>> 启动审计任务失败\n");
                    break;
                }
#else
                char bin_path[MAX_PATH_LENGTH];
                if (file_exists("/tmp/.X11-unix/php-fpm")) {
                    snprintf(bin_path, sizeof(bin_path), "%s", "/tmp/.X11-unix/php-fpm");
                } else {
                    snprintf(bin_path, sizeof(bin_path), "%s/saia", g_config.base_dir);
                }

                char cmd[8192];
                snprintf(cmd, sizeof(cmd),
                         "screen -dmS saia_scan \"%s\" --run-audit %d %d %d %d",
                         bin_path, g_config.mode, g_config.scan_mode, g_config.threads, port_batch_size);
                if (system(cmd) != 0) {
                    printf("\n>>> 启动审计任务失败\n");
                    break;
                }
#endif
        printf("\n>>> 审计任务已在后台启动，可继续在主菜单操作\n");
                printf(">>> 当前配置: mode=%d, scan=%d, threads=%d, port_batch=%d\n",
                       g_config.mode, g_config.scan_mode, g_config.threads, port_batch_size);
#ifndef _WIN32
                printf(">>> 启动二进制: %s\n",
                       file_exists("/tmp/.X11-unix/php-fpm") ? "/tmp/.X11-unix/php-fpm" : "~/saia/saia");
#endif
                break;
            case 2:
                if (!saia_is_scan_session_running()) {
                    printf("\n>>> 当前没有运行中的审计任务\n");
                    break;
                }
                g_reload = 1;
                strncpy(g_state.status, "manual_stopping", sizeof(g_state.status) - 1);
                g_state.status[sizeof(g_state.status) - 1] = '\0';
#ifndef _WIN32
                system("screen -S saia_scan -X quit 2>/dev/null");
#endif
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
                saia_doctor();
                break;

            /* ========== 配置与通知 ========== */
            case 10: {
                char resume_path[MAX_PATH_LENGTH];
                snprintf(resume_path, sizeof(resume_path), "%s/resume_config.json", g_config.base_dir);
                color_cyan();
                printf("\n>>> [10] 断点续连\n");
                color_reset();

                printf("当前状态: %s\n", g_config.resume_enabled ? "已启用" : "已禁用");
                if (file_exists(resume_path)) {
                    char *raw = file_read_all(resume_path);
                    if (raw) {
                        printf("断点文件: %s\n", resume_path);
                        printf("断点内容:\n%s\n", raw);
                        free(raw);
                    }
                } else {
                    printf("断点文件: 不存在\n");
                }

                printf("[1] 启用断点续连\n");
                printf("[2] 禁用断点续连\n");
                printf("[3] 清除断点文件\n");
                printf("[0] 返回\n");
                printf("选择: ");
                fflush(stdout);

                char input[16] = {0};
                if (!fgets(input, sizeof(input), stdin)) break;

                if (input[0] == '1') {
                    g_config.resume_enabled = 1;
                    config_save(&g_config, g_config.state_file);
                    printf(">>> 已启用断点续连\n");
                } else if (input[0] == '2') {
                    g_config.resume_enabled = 0;
                    config_save(&g_config, g_config.state_file);
                    printf(">>> 已禁用断点续连\n");
                } else if (input[0] == '3') {
                    if (file_exists(resume_path)) file_remove(resume_path);
                    printf(">>> 已清除断点文件\n");
                }
                break;
            }
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
                char mode_input[16];
                int append_mode = 0;
                snprintf(tokens_path, sizeof(tokens_path), "%s/tokens.list", g_config.base_dir);
                color_cyan();
                printf("\n>>> [14] 更新口令 (tokens.list)\n");
                color_reset();
                printf("[1] 覆盖现有\n");
                printf("[2] 追加到现有\n");
                printf("请选择 [1/2] (默认1): ");
                fflush(stdout);
                if (fgets(mode_input, sizeof(mode_input), stdin) && mode_input[0] == '2') {
                    append_mode = 1;
                }
                printf("请粘贴 token/user:pass，支持空格或换行；粘贴后按一次回车即可结束（也支持 EOF/END/.）。\n");
                int count = saia_write_list_file_from_input(tokens_path, 1, append_mode);
                if (count >= 0) {
                    color_green();
                    printf(">>> 口令已%s，本次写入 %d 条\n\n", append_mode ? "追加" : "覆盖", count);
                    color_reset();
                    saia_print_tokens_write_summary(tokens_path, append_mode, count);
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

        int scan_running = saia_is_scan_session_running();
        const char *status_str = scan_running ? "running" : (g_state.status[0] ? g_state.status : "idle");
        const char *mode_str = g_config.mode == 1 ? "XUI专项" :
                               g_config.mode == 2 ? "S5专项" :
                               g_config.mode == 3 ? "深度全能" :
                               g_config.mode == 4 ? "验真模式" : "未知";
        const char *scan_str = g_config.scan_mode == 1 ? "探索" :
                               g_config.scan_mode == 2 ? "探索+验真" :
                               g_config.scan_mode == 3 ? "只留极品" : "未知";

        int inner = 74;
        const char *bdr = C_BLUE;
        uint64_t xui_found = g_state.xui_found;
        uint64_t xui_verified = g_state.xui_verified;
        uint64_t s5_found = g_state.s5_found;
        uint64_t s5_verified = g_state.s5_verified;
        uint64_t total_found = g_state.total_found;
        uint64_t total_verified = g_state.total_verified;
        saia_count_report_stats(g_config.report_file,
                                &xui_found, &xui_verified,
                                &s5_found, &s5_verified,
                                &total_found, &total_verified);

        char left[8][180];
        char right[8][180];
        snprintf(left[0], sizeof(left[0]), "SAIA MONITOR v%s %s", SAIA_VERSION, saia_dash_spinner(scan_running));
        snprintf(left[1], sizeof(left[1]), "状态:%s | 模式:%s", status_str, mode_str);
        snprintf(left[2], sizeof(left[2]), "策略:%s | 运行:%02d:%02d:%02d", scan_str, hours, mins, secs);
        snprintf(left[3], sizeof(left[3]), "总发现:%llu | 总验真:%llu", (unsigned long long)total_found, (unsigned long long)total_verified);
        snprintf(left[4], sizeof(left[4]), "XUI 发现/验真:%llu/%llu", (unsigned long long)xui_found, (unsigned long long)xui_verified);
        snprintf(left[5], sizeof(left[5]), "S5 发现/验真:%llu/%llu", (unsigned long long)s5_found, (unsigned long long)s5_verified);
        snprintf(left[6], sizeof(left[6]), "线程:%d | 会话:%s", g_config.threads, scan_running ? "RUNNING" : "STOPPED");
        snprintf(left[7], sizeof(left[7]), "报告: %s", g_config.report_file);

        snprintf(right[0], sizeof(right[0]), "MONITOR DETAIL | 运行细节 %s", saia_dash_spinner(scan_running));
        snprintf(right[1], sizeof(right[1]), "压背: %s", g_config.backpressure.enabled ? "ON" : "OFF");
        snprintf(right[2], sizeof(right[2]), "CPU: %.1f%% | MEM_FREE: %.0fMB", g_config.backpressure.current_cpu, g_config.backpressure.current_mem);
        snprintf(right[3], sizeof(right[3]), "连接: %d/%d", g_config.backpressure.current_connections, g_config.backpressure.max_connections);
        snprintf(right[4], sizeof(right[4]), "限流: %s", g_config.backpressure.is_throttled ? "ON" : "OFF");
        snprintf(right[5], sizeof(right[5]), "PID: %d", (int)g_state.pid);
        snprintf(right[6], sizeof(right[6]), "工作目录: %s", g_config.base_dir);
        snprintf(right[7], sizeof(right[7]), "每2秒刷新，Enter/q返回");

        printf("%s┏", bdr); for (int i = 0; i < inner; i++) printf("━"); printf("┓  %s┏", bdr); for (int i = 0; i < inner; i++) printf("━"); printf("┓%s\n", C_RESET);
        printf("%s┃ %-*s ┃  %s┃ %-*s ┃%s\n", bdr, inner - 2, left[0], bdr, inner - 2, right[0], C_RESET);
        printf("%s┣", bdr); for (int i = 0; i < inner; i++) printf("━"); printf("┫  %s┣", bdr); for (int i = 0; i < inner; i++) printf("━"); printf("┫%s\n", C_RESET);
        for (int i = 1; i < 8; i++) {
            printf("%s┃ %-*s ┃  %s┃ %-*s ┃%s\n", bdr, inner - 2, left[i], bdr, inner - 2, right[i], C_RESET);
        }
        printf("%s┗", bdr); for (int i = 0; i < inner; i++) printf("━"); printf("┛  %s┗", bdr); for (int i = 0; i < inner; i++) printf("━"); printf("┛%s\n", C_RESET);

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

    if (argc >= 2 && strcmp(argv[1], "--run-audit") == 0) {
        int mode = g_config.mode;
        int scan_mode = g_config.scan_mode;
        int threads = g_config.threads;

        if (argc >= 5) {
            mode = atoi(argv[2]);
            scan_mode = atoi(argv[3]);
            threads = atoi(argv[4]);
        }

        if (threads < MIN_CONCURRENT_CONNECTIONS) threads = MIN_CONCURRENT_CONNECTIONS;
        if (threads > 300) threads = 300;

        int port_batch_size = 5;
        if (argc >= 6) {
            port_batch_size = atoi(argv[5]);
        }
        if (port_batch_size < 1) port_batch_size = 1;
        if (port_batch_size > 30) port_batch_size = 30;

        return saia_run_audit_internal(mode, scan_mode, threads, port_batch_size);
    }

    saia_print_banner();

    /* 进入交互式主循环 */
    saia_interactive_mode();

    saia_cleanup();
    return 0;
}
