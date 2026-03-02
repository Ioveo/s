CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE

# Windows特殊标志
ifeq ($(OS),Windows_NT)
    CFLAGS += -DWIN32_LEAN_AND_MEAN
    LDFLAGS = -lws2_32 -lwinhttp
    RM = del /Q
    EXE_EXT = .exe
else
    # Linux/FreeBSD
    LDFLAGS = -lpthread -lm
    RM = rm -f
    EXE_EXT =
endif

# 源文件
SRCS = main.c \
       missing_functions.c \
       config.c \
       file_ops.c \
       network.c \
       http.c \
       scanner.c \
       json_parser.c \
       utils.c \
       color.c \
       string_ops.c \
       backpressure.c

# 目标文件
OBJS = $(SRCS:.c=.o)

# 最终可执行文件
TARGET = saia$(EXE_EXT)

# 默认目标
all: $(TARGET)

# 链接规则
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "编译成功! 生成: $(TARGET)"

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 运行
run: $(TARGET)
	./$(TARGET)

# 清理
clean:
	$(RM) $(OBJS) $(TARGET)

# 清理所有（包括状态文件）
distclean: clean
ifeq ($(OS),Windows_NT)
	$(RM) *.json *.log *.lock *.list *.tmp
else
	$(RM) *.json *.log *.lock *.list *.tmp
endif

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: clean all

.PHONY: all run clean distclean debug
