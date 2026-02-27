# Menu Input Enhancement Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Enhance the C version's menu to support bulk adding IPs and Credentials with auto-splitting and comment removal, matching the Python version's UX.

**Architecture:** Implement a utility function `read_multiline_input` in `utils.c` that handles multi-line input until EOF, splits by whitespace/newlines, and removes comments starting with `#`. Integrate this into `main.c` menu handlers.

**Tech Stack:** C (Standard Lib), Stdio.

---

### Task 1: Implement Input Parsing Utility

**Files:**
- Modify: `saia.h`
- Modify: `utils.c`

**Step 1: Add declaration to header**

In `saia.h`:
```c
// Add under string_ops or utils section
int read_multiline_input(char ***out_lines, size_t *out_count);
void free_multiline_input(char **lines, size_t count);
```

**Step 2: Implement logic in utils.c**

```c
#include <ctype.h>

int read_multiline_input(char ***out_lines, size_t *out_count) {
    if (!out_lines || !out_count) return -1;
    
    *out_lines = NULL;
    *out_count = 0;
    
    size_t capacity = 16;
    char **lines = (char **)malloc(capacity * sizeof(char *));
    if (!lines) return -1;
    
    printf("请输入内容 (自动去除 # 注释，支持换行，单独输入 EOF 结束):\n");
    
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        // Remove comments
        char *comment = strchr(buffer, '#');
        if (comment) *comment = '\0';
        
        // Check for EOF keyword
        if (strncmp(buffer, "EOF", 3) == 0) break;
        
        // Split by whitespace
        char *p = buffer;
        while (*p) {
            // Skip leading spaces
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            
            // Find end of word
            char *end = p;
            while (*end && !isspace((unsigned char)*end)) end++;
            
            size_t len = end - p;
            if (len > 0) {
                if (*out_count >= capacity) {
                    capacity *= 2;
                    char **new_lines = (char **)realloc(lines, capacity * sizeof(char *));
                    if (!new_lines) {
                        // Cleanup on failure
                        for(size_t i=0; i<*out_count; i++) free(lines[i]);
                        free(lines);
                        return -1;
                    }
                    lines = new_lines;
                }
                
                lines[*out_count] = (char *)malloc(len + 1);
                strncpy(lines[*out_count], p, len);
                lines[*out_count][len] = '\0';
                (*out_count)++;
            }
            
            p = end;
        }
    }
    
    *out_lines = lines;
    return 0;
}

void free_multiline_input(char **lines, size_t count) {
    if (!lines) return;
    for(size_t i=0; i<count; i++) {
        if(lines[i]) free(lines[i]);
    }
    free(lines);
}
```

### Task 2: Update Node Management Menu

**Files:**
- Modify: `main.c`

**Step 1: Update `saia_nodes_menu` case 1**

Replace existing `fgets` loop with:

```c
        case 1: {
            char **inputs = NULL;
            size_t count = 0;
            if (read_multiline_input(&inputs, &count) == 0 && count > 0) {
                FILE *f = fopen(nodes_path, "a");
                if (f) {
                    for (size_t i = 0; i < count; i++) {
                        fprintf(f, "%s\n", inputs[i]);
                        printf("  已添加: %s\n", inputs[i]);
                    }
                    fclose(f);
                    printf("成功添加 %zu 个节点\n", count);
                } else {
                    printf("无法打开文件: %s\n", nodes_path);
                }
                free_multiline_input(inputs, count);
            } else {
                printf("未输入有效内容\n");
            }
            break;
        }
```

### Task 3: Update Credentials Management Menu

**Files:**
- Modify: `main.c`

**Step 1: Update `saia_credentials_menu` case 1**

```c
        case 1: {
            char **inputs = NULL;
            size_t count = 0;
            if (read_multiline_input(&inputs, &count) == 0 && count > 0) {
                FILE *f = fopen(tokens_path, "a");
                if (f) {
                    for (size_t i = 0; i < count; i++) {
                        // Check logic: if contains ':', keep as is; else prepend 'admin:'
                        if (strchr(inputs[i], ':')) {
                            fprintf(f, "%s\n", inputs[i]);
                            printf("  已添加: %s\n", inputs[i]);
                        } else {
                            fprintf(f, "admin:%s\n", inputs[i]);
                            printf("  已添加: admin:%s\n", inputs[i]);
                        }
                    }
                    fclose(f);
                    printf("成功添加 %zu 个凭据\n", count);
                } else {
                    printf("无法打开文件: %s\n", tokens_path);
                }
                free_multiline_input(inputs, count);
            } else {
                printf("未输入有效内容\n");
            }
            break;
        }
```
