# 编译器
CC = gcc

# 定义 Unity 输出字符宏
DEFINES = -D UNITY_OUTPUT_CHAR=UnityOutputCharSpy_OutputChar
ifeq ($(OS),Windows_NT)
  DEFINES += -D UNITY_OUTPUT_CHAR_HEADER_DECLARATION=UnityOutputCharSpy_OutputChar\(int\)
endif

# 源文件
SRC = unity_memory.c \
      ../../Unity/unity.c \
	  ../../../xcore/xhal_malloc.c \
      unity_memory_Test.c \
      unity_memory_TestRunner.c \
      unity_output_Spy.c

# 头文件路径
INC_DIR = -I../../Unity/ -I../../../xcore/

# 输出目录和目标
BUILD_DIR = build
TARGET = build/memory_tests.exe

# 默认目标
all: default

default: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEFINES) $(SRC) $(INC_DIR) -std=c99 -o $(TARGET) -D UNITY_MALLOC=xmalloc -D UNITY_FREE=xfree
	@echo "default build"
	./$(TARGET)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET) $(BUILD_DIR)/*.gc*
	rm -f *.gc*

cov: $(BUILD_DIR)
	$(CC) $(DEFINES) $(SRC) $(INC_DIR) -o $(TARGET) -std=c99 -fprofile-arcs -ftest-coverage
	rm -f *.gcda
	./$(TARGET) > /dev/null ; ./$(TARGET) -v > /dev/null
	cd $(BUILD_DIR) && \
	gcov ../xhal_malloc.c | head -3
	cd $(BUILD_DIR) && \
	grep '###' xhal_malloc.c.gcov -C2 || true # Show uncovered lines

# 额外警告选项
CFLAGS += -Wbad-function-cast
CFLAGS += -Wcast-qual
# CFLAGS += -Wconversion
CFLAGS += -Wformat=2
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wold-style-definition
CFLAGS += -Wpointer-arith
CFLAGS += -Wshadow
CFLAGS += -Wstrict-overflow=5
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wswitch-default
CFLAGS += -Wundef
CFLAGS += -Wno-error=undef
CFLAGS += -Wunreachable-code
CFLAGS += -Wunused
CFLAGS += -fstrict-aliasing
