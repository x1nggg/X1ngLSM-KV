#!/bin/bash
# X1ngLSM-KV 清理脚本
# 用法: ./scripts/clean.sh [all]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "${GREEN}=== X1ngLSM-KV 清理脚本 ===${NC}"
echo

# 解析参数
CLEAN_ALL=false
if [[ "$1" == "all" ]]; then
    CLEAN_ALL=true
fi

# 清理构建目录
BUILD_DIR="${PROJECT_ROOT}/build"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}清理构建目录...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}✓ ${BUILD_DIR} 已删除${NC}"
else
    echo -e "${YELLOW}构建目录不存在${NC}"
fi

# 清理数据目录
if [ "$CLEAN_ALL" = true ]; then
    DATA_DIR="${PROJECT_ROOT}/data"
    if [ -d "$DATA_DIR" ]; then
        echo -e "${YELLOW}清理数据目录...${NC}"
        rm -rf "$DATA_DIR"
        echo -e "${GREEN}✓ ${DATA_DIR} 已删除${NC}"
    fi

    # 清理其他生成文件
    echo -e "${YELLOW}清理其他生成文件...${NC}"
    find "$PROJECT_ROOT" -type f -name "*.o" -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -type f -name "*.a" -delete 2>/dev/null || true
    echo -e "${GREEN}✓ 生成文件已清理${NC}"
fi

echo
echo -e "${GREEN}=== 清理完成 ===${NC}"
