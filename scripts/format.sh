#!/bin/bash
# X1ngLSM-KV 代码格式化脚本
# 用法: ./scripts/format.sh [check|fix]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "${GREEN}=== X1ngLSM-KV 代码格式化 ===${NC}"
echo

# 解析参数
MODE="fix"
if [[ "$1" == "check" ]]; then
    MODE="check"
fi

# 检查 clang-format
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}错误: clang-format 未安装${NC}"
    echo
    echo "安装方法:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-format"
    echo "  macOS: brew install clang-format"
    echo "  Windows: winget install LLVM.clang-format"
    exit 1
fi

# 查找需要格式化的文件
echo -e "${YELLOW}查找源文件...${NC}"
FILES=(
    "$PROJECT_ROOT"/src/**/*.cpp
    "$PROJECT_ROOT"/src/**/*.h
    "$PROJECT_ROOT"/include/**/*.hpp
    "$PROJECT_ROOT"/test/**/*.cpp
    "$PROJECT_ROOT"/examples/**/*.cpp
)

# 检查模式
if [ "$MODE" == "check" ]; then
    echo -e "${YELLOW}检查代码格式...${NC}"
    echo

    ISSUES=0
    for file in "${FILES[@]}"; do
        if [ -f "$file" ]; then
            if ! clang-format --dry-run --Werror "$file" &> /dev/null; then
                echo -e "${RED}✗ $file${NC}"
                ((ISSUES++))
            fi
        fi
    done

    echo
    if [ $ISSUES -eq 0 ]; then
        echo -e "${GREEN}=== 所有文件格式正确 ===${NC}"
    else
        echo -e "${RED}=== $ISSUES 个文件需要格式化 ===${NC}"
        echo "运行: $0 fix"
        exit 1
    fi
else
    # 修复模式
    echo -e "${YELLOW}格式化代码...${NC}"
    echo

    for file in "${FILES[@]}"; do
        if [ -f "$file" ]; then
            echo "格式化: $file"
            clang-format -i "$file"
        fi
    done

    echo
    echo -e "${GREEN}=== 格式化完成 ===${NC}"
fi
