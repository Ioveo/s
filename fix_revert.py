import re


def fix_duplicate_cases():
    with open("main.c", "r", encoding="utf-8") as f:
        content = f.read()

    # It seems the replacement logic applied "File Import" logic to ALL "case 2: {" blocks!
    # We only wanted it for saia_credentials_menu.

    # We need to revert the incorrect replacements.

    lines = content.splitlines()
    new_lines = []

    # Helper to check if we are inside a specific function
    current_function = ""

    i = 0
    while i < len(lines):
        line = lines[i]

        # Track function scope
        if line.startswith("int saia_") and line.endswith("{"):
            current_function = line.split("(")[0].split()[-1]

        if "case 2: {" in line:
            # Check context
            if current_function == "saia_credentials_menu":
                # This is correct for File Import
                new_lines.append(line)
                i += 1
                continue

            elif current_function == "saia_nodes_menu":
                # This should be "Add IP Nodes" (Append)
                new_lines.append(line)
                new_lines.append("            color_cyan();")
                new_lines.append('            printf(">>> 添加 IP 节点\\n");')
                new_lines.append("            color_reset();")
                new_lines.append(
                    "            int count = saia_write_list_file_from_input(nodes_path, 1, 1);"
                )
                new_lines.append("            if (count >= 0) {")
                new_lines.append("                color_green();")
                new_lines.append(
                    '                printf(">>> 节点已追加，本次追加 %d 条\\n", count);'
                )
                new_lines.append("                color_reset();")
                new_lines.append("            } else {")
                new_lines.append("                color_red();")
                new_lines.append('                printf(">>> 追加失败或已取消\\n");')
                new_lines.append("                color_reset();")
                new_lines.append("            }")
                new_lines.append("            break;")
                new_lines.append("        }")

                # Skip the incorrect "File Import" block that follows
                # Skip until "break;" and "}"
                # The incorrect block is about 33 lines long
                while i < len(lines):
                    if lines[i].strip() == "break;" and lines[i + 1].strip() == "}":
                        i += 2
                        break
                    i += 1
                continue

            elif current_function == "saia_telegram_menu":
                # This should be "Configure Bot"
                # Need to restore original logic
                new_lines.append(line)
                new_lines.append("            char input[512];")
                new_lines.append('            printf("Bot Token: ");')
                new_lines.append("            fgets(input, sizeof(input), stdin);")
                new_lines.append("            input[strcspn(input, \"\\n\")] = '\\0';")
                new_lines.append(
                    "            strncpy(g_config.telegram_token, input, sizeof(g_config.telegram_token) - 1);"
                )
                new_lines.append('            printf("Chat ID: ");')
                new_lines.append("            fgets(input, sizeof(input), stdin);")
                new_lines.append("            input[strcspn(input, \"\\n\")] = '\\0';")
                new_lines.append(
                    "            strncpy(g_config.telegram_chat_id, input, sizeof(g_config.telegram_chat_id) - 1);"
                )
                new_lines.append('            printf("推送间隔(分钟): ");')
                new_lines.append("            fgets(input, sizeof(input), stdin);")
                new_lines.append(
                    "            g_config.telegram_interval = atoi(input);"
                )
                new_lines.append('            printf("配置已完成\\n");')
                new_lines.append("            break;")
                new_lines.append("        }")

                # Skip incorrect block
                while i < len(lines):
                    if lines[i].strip() == "break;" and lines[i + 1].strip() == "}":
                        i += 2
                        break
                    i += 1
                continue

            elif current_function == "saia_backpressure_menu":
                # This should be "Configure Thresholds"
                new_lines.append(line)
                new_lines.append("            char input[256];")
                new_lines.append(
                    '            printf("CPU阈值 [%%] [当前=%.1f]: ", g_config.backpressure.cpu_threshold);'
                )
                new_lines.append("            fgets(input, sizeof(input), stdin);")
                new_lines.append(
                    "            if (strlen(input) > 1) g_config.backpressure.cpu_threshold = atof(input);"
                )
                new_lines.append(
                    '            printf("内存阈值 [MB] [当前=%.1f]: ", g_config.backpressure.mem_threshold);'
                )
                new_lines.append("            fgets(input, sizeof(input), stdin);")
                new_lines.append(
                    "            if (strlen(input) > 1) g_config.backpressure.mem_threshold = atof(input);"
                )
                new_lines.append("            break;")
                new_lines.append("        }")

                # Skip incorrect block
                while i < len(lines):
                    if lines[i].strip() == "break;" and lines[i + 1].strip() == "}":
                        i += 2
                        break
                    i += 1
                continue

        new_lines.append(line)
        i += 1

    with open("main.c", "w", encoding="utf-8") as f:
        f.writelines([l + "\n" if not l.endswith("\n") else l for l in new_lines])


if __name__ == "__main__":
    fix_duplicate_cases()
    print("Reverted incorrect case replacements")
