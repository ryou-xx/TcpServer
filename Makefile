# ==============================================================================
# 编译和链接设置
# ==============================================================================

# 编译器
CXX = g++

# 编译选项
# -std=c++17:      使用 C++17 标准
# -g:              生成调试信息
# -Wall:           开启所有警告
CXXFLAGS = -std=c++17 -g

# 链接选项
# -lpthread:       链接 POSIX 线程库
# -ljsoncpp:       链接 jsoncpp 库
# *** 修改1：将链接器标志从 CXXFLAGS 中分离出来 ***
LDFLAGS = -lpthread -ljsoncpp

# ==============================================================================
# 目录和文件定义
# ==============================================================================

# 目录定义
SRCDIR = src
OBJDIR = obj
INCDIR = include
LOG_INCDIR_1 = log_system/logs_code
LOG_INCDIR_2 = log_system/logs_code/backlog

# 将所有头文件搜索路径整合到一个变量中
INCLUDE_PATHS = -I$(INCDIR) -I$(LOG_INCDIR_1) -I$(LOG_INCDIR_2)

# 目标可执行文件名
# *** 修改2：将目标文件放在项目根目录，而不是 src 目录中 ***
TARGET = test

# *** 修改3：自动查找所有目录下的 .cpp 源文件 ***
# 使用 wildcard 搜索你提到的所有包含 .cpp 文件的目录
SOURCES = $(wildcard $(SRCDIR)/*.cpp) \
          $(wildcard $(LOG_INCDIR_1)/MyLog.cpp) \
          $(wildcard $(LOG_INCDIR_2)/CliBackupLog.cpp)

# *** 修改4：更健壮地生成目标文件名 ***
# $(notdir ...) 会去掉源文件的目录路径 (例如 src/Acceptor.cpp -> Acceptor.cpp)
# 然后我们为每个文件名添加 obj/ 前缀和 .o 后缀
# 这样可以避免不同目录下的同名源文件导致冲突 (虽然你现在没有)
OBJECTS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(notdir $(SOURCES)))

# *** 修改5：告诉 make 在哪里可以找到源文件 ***
# VPATH 是一个特殊变量，make 会在这些目录中搜索依赖文件
VPATH = $(SRCDIR) $(LOG_INCDIR_1) $(LOG_INCDIR_2)

# ==============================================================================
# 规则定义 (RULES)
# ==============================================================================

# 默认目标 (当执行 make 时，会执行这个目标)
all: $(TARGET)

# 生成可执行文件的规则
# $@ 代表目标文件 (test)
# $^ 代表所有依赖文件 (所有的 .o 文件)
# *** 修改6：在链接时加上 LDFLAGS ***
$(TARGET): $(OBJECTS)
	@echo "Linking to create executable: $@"
	$(CXX) -o $@ $^ $(LDFLAGS)
	@echo "Successfully created executable: $(TARGET)"

# 生成目标文件的规则 (通用模式规则)
# $< 代表第一个依赖文件 (即找到的 .cpp 源文件)
# -c 表示只编译，不链接
# *** 修改7：使用通用规则，配合 VPATH 查找源文件 ***
$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(OBJDIR) # 确保 obj 目录存在
	@echo "Compiling $< -> $@"
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) -c -o $@ $<

# ==============================================================================
# 清理规则 (CLEANUP)
# ==============================================================================

# .PHONY 声明一个“伪目标”，这个目标不代表一个真实的文件
.PHONY: all clean

# 清理生成的文件
clean:
	@echo "Cleaning up generated files..."
	rm -rf $(OBJDIR) $(TARGET)
	@echo "Cleanup complete."