def fix_file_content():
    with open("main.c", "r", encoding="utf-8") as f:
        lines = f.readlines()

    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]

        # 1. Fix: if (len > 0 && content[len - 1] != \n') {
        if "content[len - 1] != \\n')" in line:
            new_lines.append(
                line.replace("content[len - 1] != \\n')", "content[len - 1] != '\\n')")
            )
            i += 1
            continue

        # 2. Fix: if (comment) *comment = '';
        if "*comment = '';" in line:
            new_lines.append(line.replace("*comment = '';", "*comment = '\\0';"))
            i += 1
            continue

        # 3. Fix: buffer[strcspn(buffer, "
        # 4.      ")] = '';
        if 'buffer[strcspn(buffer, "' in line:
            # Look ahead for closing
            if i + 2 < len(lines) and '")]' in lines[i + 2]:
                # Merge lines
                # We want: buffer[strcspn(buffer, "\n")] = '\0';
                new_lines.append("        buffer[strcspn(buffer, \"\\n\")] = '\\0';\n")
                i += 3
                continue

        # 5. Fix: char *token = strtok(trimmed, "
        # 6.      ");
        if 'char *token = strtok(trimmed, "' in line:
            if i + 2 < len(lines) and '");' in lines[i + 2]:
                new_lines.append(
                    '            char *token = strtok(trimmed, " \\t\\n");\n'
                )
                i += 3
                continue

        # 7. Fix: fprintf(fp, "%s\n\n, token);
        if 'fprintf(fp, "%s\\n\\n, token);' in line:
            new_lines.append('                    fprintf(fp, "%s\\n", token);\n')
            i += 1
            continue

        # 8. Fix: token = strtok(NULL, "
        # 9.      ");
        if 'token = strtok(NULL, "' in line:
            if i + 2 < len(lines) and '");' in lines[i + 2]:
                new_lines.append('                token = strtok(NULL, " \\t\\n");\n')
                i += 3
                continue

        # 10. Fix: fprintf(fp, "%s\n\n, trimmed);
        if 'fprintf(fp, "%s\\n\\n, trimmed);' in line:
            new_lines.append('                fprintf(fp, "%s\\n", trimmed);\n')
            i += 1
            continue

        new_lines.append(line)
        i += 1

    # Remove excessive empty lines to clean up
    final_lines = []
    for line in new_lines:
        if line.strip() == "":
            if final_lines and final_lines[-1].strip() == "":
                continue
        final_lines.append(line)

    with open("main.c", "w", encoding="utf-8") as f:
        f.writelines(final_lines)


if __name__ == "__main__":
    fix_file_content()
    print("Fixed syntax errors")
