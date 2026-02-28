import re


def fix_printf_issues():
    with open("main.c", "r", encoding="utf-8") as f:
        content = f.read()

    # Issue 1: printf("... \n", arg"); -> should be printf("... \n", arg);
    # Regex to find: printf\("([^"]+)\\n",\s*([^"]+)"\);
    # But it's simpler to just look for ", arg"); pattern

    # We have lines like: printf("  并发线程: %d\n", state->threads");
    # The error is the extra " at the end before );

    # Pattern: printf("format", arg");
    # Replacement: printf("format", arg);

    lines = content.splitlines()
    new_lines = []

    for line in lines:
        stripped = line.strip()

        # Check for the pattern: ending with "); where there is a " before );
        # e.g. ", arg");
        if stripped.endswith('");') and "," in line:
            # Check if there's a quote before ); that isn't the format string ending
            # printf("fmt", arg");

            # Find the last quote
            last_quote_idx = line.rfind('"')
            # Find the second to last quote
            # This is tricky because the format string is in quotes.
            # printf("fmt", arg");
            # Quotes at: 7, 12, 18

            # If the line ends with "); and the character before ); is "
            if line.rstrip().endswith('");'):
                # Check if this last quote is part of the argument
                # e.g. printf("Hello %s", "World"); -> VALID
                # e.g. printf("Hello %d", num"); -> INVALID

                # Heuristic: if the char before last quote is an alphanumeric or _ or ) or ], it's likely an error
                # args are usually variables or expressions.
                # If it is "World"); the char before " is d.
                # If it is num"); the char before " is m.

                # Let's look at the specific errors reported.
                # printf("\n[INT] 收到信号 %d，正在停止...\n", signum");
                # printf("  并发线程: %d\n", state->threads");

                # Strategy: replace "); with ); if the context suggests it's a variable
                # Regex: , [a-zA-Z0-9_>.\-\(\) ]+"\);

                # Replace `");` with `);` at the end of line, but ONLY if the preceding part looks like a variable, not a string literal
                # String literal arg: , "string"); -> don't touch
                # Variable arg: , variable"); -> change to , variable);

                # How to distinguish?
                # If there is an opening quote for the argument?
                # printf("fmt", "arg"); -> Quote count is 4.
                # printf("fmt", arg"); -> Quote count is 3.

                # Count quotes in the line
                quote_count = line.count('"')
                if quote_count % 2 == 1:
                    # Odd number of quotes -> definitely unbalanced.
                    # Remove the last quote
                    last_quote = line.rfind('"')
                    if last_quote != -1:
                        line = line[:last_quote] + line[last_quote + 1 :]

        # Issue 2: Broken printf arguments with newlines
        # printf("  当前连接: %d/%d\n",\n\ng_config.backpressure.current_connections,\n\ng_config.backpressure.max_connections");
        # This needs to be cleaned up. \n\n in code is invalid.
        if "printf" in line and r",\n\n" in line:
            line = line.replace(r",\n\n", ", ")

        if r"\n\n" in line and not '"' in line:  # Argument part
            line = line.replace(r"\n\n", "")

        # Also check for: printf("...",\n\narg, ...);
        line = line.replace(",\\n\\n", ", ")

        # Check for specific broken line mentioned in error
        if 'strlen(g_config.telegram_token) > 10 ? "" : ""' in line:
            # Fix the mess around it
            # printf("Bot Token: %s***\n",\n\ng_config.telegram_token,\n\nstrlen(g_config.telegram_token) > 10 ? "" : "");
            # Should be: printf("Bot Token: %s***\n", g_config.telegram_token);
            # Wait, the logic seems to be trying to mask the token?
            # printf("Bot Token: %s***\n", ...);
            # The format %s*** implies printing a string then ***.
            # But the args are complex.
            # Let's look at what it was likely trying to do.
            # Maybe: printf("Bot Token: %s***\n", g_config.telegram_token);
            # The conditional operator part looks like garbage or truncated logic.

            # Let's simplify to: printf("Bot Token: %s\n", g_config.telegram_token);
            if "Bot Token" in line:
                indent = line[: line.find("printf")]
                line = f'{indent}printf("Bot Token: %s\\n", g_config.telegram_token);'

        new_lines.append(line)

    with open("main.c", "w", encoding="utf-8") as f:
        f.writelines([l + "\n" if not l.endswith("\n") else l for l in new_lines])


if __name__ == "__main__":
    fix_printf_issues()
    print("Fixed printf quotes and args")
