#!/bin/bash
# X1ngLSM-KV CI/CD 模拟脚本
# 用法: ./scripts/ci.sh

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "${GREEN}=== X1ngLSM-KV CI/CD ===${NC}"
echo

START_TIME=$(date +%s)

# 阶段 1: 清理
echo -e "${BLUE}[1/5] 清理...${NC}"
"${PROJECT_ROOT}/scripts/clean.sh" all

# 阶段 2: 构建
echo -e "${BLUE}[2/5] 构建 (Release)...${NC}"
"${PROJECT_ROOT}/scripts/build.sh" release

# 阶段 3: 格式检查
echo -e "${BLUE}[3/5] 格式检查...${NC}"
"${PROJECT_ROOT}/scripts/format.sh" check

# 阶段 4: 运行测试
echo -e "${BLUE}[4/5] 运行测试...${NC}"
TEST_DIR="${PROJECT_ROOT}/bin/test"
FAILED=0
for test_bin in "$TEST_DIR"/test_*; do
    if [ -x "$test_bin" ]; then
        echo -e "${YELLOW}运行: $(basename "$test_bin")${NC}"
        if "$test_bin"; then
            echo -e "${GREEN}✓ $(basename "$test_bin") 通过${NC}"
        else
            echo -e "${RED}✗ $(basename "$test_bin") 失败${NC}"
            FAILED=1
        fi
    fi
done
if [ $FAILED -ne 0 ]; then
    echo -e "${RED}=== 测试失败 ===${NC}"
    exit 1
fi
echo -e "${GREEN}=== 所有测试通过 ===${NC}"

# 阶段 5: 代码统计
echo -e "${BLUE}[5/5] 代码统计...${NC}"
echo

# 统计代码行数
echo "代码统计:"
echo "----------"
echo -n "头文件: "
find "$PROJECT_ROOT/include" -name "*.hpp" -o -name "*.h" | xargs wc -l 2>/dev/null | tail -1 || echo "0"
echo -n "源文件: "
find "$PROJECT_ROOT/src" -name "*.cpp" | xargs wc -l 2>/dev/null | tail -1 || echo "0"
echo -n "测试代码: "
find "$PROJECT_ROOT/test" -name "*.cpp" | xargs wc -l 2>/dev/null | tail -1 || echo "0"
echo -n "示例代码: "
find "$PROJECT_ROOT/examples" -name "*.cpp" | xargs wc -l 2>/dev/null | tail -1 || echo "0"

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo
echo -e "${GREEN}=== CI/CD 成功完成 (用时: ${DURATION}s) ===${NC}"
