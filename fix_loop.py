import re


def fix_input_loop():
    with open("main.c", "r", encoding="utf-8") as f:
        content = f.read()

    # We want to insert `if (!g_running) break;` inside the while loop of saia_write_list_file_from_input

    # Find the function
    # static int saia_write_list_file_from_input(...) { ... while (fgets(...)) {

    # We can match the while loop line
    # while (fgets(buffer, sizeof(buffer), stdin)) {

    # And replace it with:
    # while (fgets(buffer, sizeof(buffer), stdin)) {
    #     if (!g_running) break;

    new_content = content.replace(
        "while (fgets(buffer, sizeof(buffer), stdin)) {",
        "while (fgets(buffer, sizeof(buffer), stdin)) {\n        if (!g_running) break;",
    )

    with open("main.c", "w", encoding="utf-8") as f:
        f.write(new_content)


if __name__ == "__main__":
    fix_input_loop()
    print("Fixed input loop termination")
