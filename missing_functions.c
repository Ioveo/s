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
            snprintf(path, sizeof(path), "%s\\sys_audit_events.log", base);
            if (file_remove(path) == 0) { removed++; }
            printf("已清理 %d 个日志文件\n", removed);
            break;

        case 2:
            snprintf(path, sizeof(path), "%s\\audit_report.log", base);
            if (file_remove(path) == 0) { removed++; }
            printf("已清理 %d 个报告文件\n", removed);
            break;

        case 3:
            snprintf(path, sizeof(path), "%s\\sys_audit_state.json", base);
            if (file_remove(path) == 0) { removed++; }
            printf("已清理 %d 个状态文件\n", removed);
            break;

        case 4:
            snprintf(path, sizeof(path), "%s\\sys_audit_events.log", base);
            if (file_remove(path) == 0) { removed++; }
            snprintf(path, sizeof(path), "%s\\audit_report.log", base);
            if (file_remove(path) == 0) { removed++; }
            snprintf(path, sizeof(path), "%s\\sys_audit_state.json", base);
            if (file_remove(path) == 0) { removed++; }
            snprintf(path, sizeof(path), "%s\\audit_runner.lock", base);
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
                saia_report_menu();
                break;

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

// ==================== 主函数 ====================

