#!/bin/bash
# X1ngLSM-KV 构建脚本
# 用法: ./scripts/build.sh [debug|release|clean]

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

echo -e "${GREEN}=== X1ngLSM-KV 构建脚本 ===${NC}"
echo "项目目录: ${PROJECT_ROOT}"
echo "构建目录: ${BUILD_DIR}"
echo

# 解析参数
BUILD_TYPE="Release"
CLEAN_ONLY=false
RUN_TESTS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        debug|Debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        release|Release)
            BUILD_TYPE="Release"
            shift
            ;;
        clean)
            CLEAN_ONLY=true
            shift
            ;;
        test)
            RUN_TESTS=true
            shift
            ;;
        *)
            echo -e "${RED}未知参数: $1${NC}"
            echo "用法: $0 [debug|release|clean|test]"
            exit 1
            ;;
    esac
done

# 清理功能
if [ "$CLEAN_ONLY" = true ]; then
    echo -e "${YELLOW}清理构建目录...${NC}"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo -e "${GREEN}✓ 构建目录已清理${NC}"
    else
        echo -e "${YELLOW}构建目录不存在，无需清理${NC}"
    fi
    exit 0
fi

# 创建构建目录
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}创建构建目录...${NC}"
    mkdir -p "$BUILD_DIR"
fi

# 配置 CMake
echo -e "${YELLOW}配置 CMake (${BUILD_TYPE})...${NC}"
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# 编译
echo -e "${YELLOW}开始编译...${NC}"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
make -j"$NPROC"

# 检查编译结果
if [ $? -eq 0 ]; then
    echo
    echo -e "${GREEN}=== 构建成功 ===${NC}"
    echo
    echo "可执行文件位置:"
    echo "  - CLI: ${PROJECT_ROOT}/bin/cli/x1nglsm-cli"
    echo "  - 示例: ${PROJECT_ROOT}/bin/examples/"
    echo "  - 测试: ${PROJECT_ROOT}/bin/test/"
    echo

    # 运行测试
    if [ "$RUN_TESTS" = true ]; then
        echo -e "${YELLOW}运行测试...${NC}"
        for test_bin in "${PROJECT_ROOT}/bin/test/"test_*; do
            if [ -x "$test_bin" ]; then
                "$test_bin"
            fi
        done
    fi
else
    echo -e "${RED}=== 构建失败 ===${NC}"
    exit 1
fi
