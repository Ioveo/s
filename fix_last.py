import re


def fix_final_errors():
    with open("main.c", "r", encoding="utf-8") as f:
        content = f.read()

    lines = content.splitlines()
    new_lines = []

    for line in lines:
        # Fix: printf("  模式: %s (%s)\n", state->mode == 1 ? "XUI专项" :\n\nstate->mode == 2 ? "S5专项" :\n\nstate->mode == 3 ? "深度全能" : "验真模式", state->work_mode == 1 ? "探索" :\n\nstate->work_mode == 2 ? "探索+验真" : "只留极品");
        # Remove \n\n inside the ternary operators
        if 'printf("  模式: %s (%s)\\n"' in line:
            line = line.replace(r"\n\n", "")

        new_lines.append(line)

    # Check if saia_telegram_menu is closed properly
    # It seems to be missing closing braces for switch and function
    # Line 1160: switch (choice) {
    # Line 1204: }  <- This closes case 2
    # We need closing for switch and function

    # Let's count braces to be sure
    brace_count = 0
    in_telegram_menu = False

    for i, line in enumerate(new_lines):
        if "int saia_telegram_menu(void) {" in line:
            in_telegram_menu = True
            brace_count = 0

        if in_telegram_menu:
            brace_count += line.count("{")
            brace_count -= line.count("}")

    # If brace_count > 0, we need to add }
    if brace_count > 0:
        new_lines.append("    }")  # Close switch
        new_lines.append("    return 0;")
        new_lines.append("}")  # Close function

    with open("main.c", "w", encoding="utf-8") as f:
        f.writelines([l + "\n" for l in new_lines])


if __name__ == "__main__":
    fix_final_errors()
    print("Fixed final errors")
