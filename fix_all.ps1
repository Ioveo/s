# 读取文件
$content = Get-Content -Path "main.c" -Raw -Encoding UTF8

# 1. 修复所有的字面量换行符导致的 printf 报错
$content = $content -replace 'printf\("支持空格/换行混合输入 \(自动去除 # 注释\)\r?\n\s*"\);', 'printf("支持空格/换行混合输入 (自动去除 # 注释)\n");'
$content = $content -replace 'printf\("请输入内容，单独输入 EOF 结束:\r?\n\s*"\);', 'printf("请输入内容，单独输入 EOF 结束:\n");'
$content = $content -replace 'fprintf\(fp, "\r?\n"\s*\);', 'fprintf(fp, "\n");'
$content = $content -replace 'fprintf\(fp, "%s\r?\n"\s*, token\);', 'fprintf(fp, "%s\n", token);'
$content = $content -replace 'fprintf\(fp, "%s\r?\n"\s*, trimmed\);', 'fprintf(fp, "%s\n", trimmed);'

# 2. 修复带有换行的字符串常量
$content = $content -replace 'strcspn\(buffer, "\r?\n"\)', 'strcspn(buffer, "\n")'
$content = $content -replace 'strtok\(trimmed, " \t\r?\n"\)', 'strtok(trimmed, " \t\n")'
$content = $content -replace 'strtok\(NULL, " \t\r?\n"\)', 'strtok(NULL, " \t\n")'

# 3. 修复带换行的消息提示
$content = $content -replace 'printf\(">>> 写入临时文件失败\r?\n"\s*\);', 'printf(">>> 写入临时文件失败\n");'
$content = $content -replace 'printf\(">>> IP 列表已更新，本次写入 %d 条\r?\n"\s*, count\);', 'printf(">>> IP 列表已更新，本次写入 %d 条\n", count);'
$content = $content -replace 'printf\(">>> 写入失败或已取消\r?\n"\s*\);', 'printf(">>> 写入失败或已取消\n");'
$content = $content -replace 'printf\(">>> 节点已追加，本次追加 %d 条\r?\n"\s*, count\);', 'printf(">>> 节点已追加，本次追加 %d 条\n", count);'
$content = $content -replace 'printf\(">>> 追加失败或已取消\r?\n"\s*\);', 'printf(">>> 追加失败或已取消\n");'
$content = $content -replace 'printf\("\r?\n"\s*\);', 'printf("\n");'

# 4. 修复 if (len > 0 && content[len - 1] != '\n') 报错
$content = $content -replace "content\[len - 1\] != '\r?\n'", "content[len - 1] != '\n'"

# 5. 修复 filter_keyword 错误
# 找到对应行并在其上一行添加定义
$content = $content -replace '(\s+)(if \(strlen\(filter_keyword\) > 0\) \{)', '$1const char *filter_keyword = "";$1$2'

# 6. 修复 "nodes_path" 变量名错误 (Line 573, 576 报错 did you mean 'file_path'?)
$content = $content -replace 'nodes_path', 'file_path'

# 7. 修复 buffer 在 block 外部使用的报错
# 我们需要确保 buffer 定义的位置足够大
$content = $content -replace "char buffer\[4096\];", "char buffer[4096];`n    char ip_buf[256];"

# 输出并保存
$content | Out-File -FilePath "main.c" -Encoding UTF8 -NoNewline
Write-Host "Fix complete!"
