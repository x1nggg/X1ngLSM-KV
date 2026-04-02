# 开发脚本

## 脚本列表

### build.sh - 构建

```bash
./scripts/build.sh              # Release 构建
./scripts/build.sh debug        # Debug 构建
./scripts/build.sh clean        # 清理构建目录
./scripts/build.sh test         # 构建并运行测试
```

### clean.sh - 清理

```bash
./scripts/clean.sh              # 清理构建目录
./scripts/clean.sh all          # 清理构建目录和数据目录
```

### format.sh - 代码格式化

```bash
./scripts/format.sh             # 格式化代码
./scripts/format.sh check       # 仅检查格式
```

需要安装 `clang-format`：

```bash
# Ubuntu/Debian
sudo apt-get install clang-format
```

### ci.sh - CI/CD

模拟完整 CI 流程，依次执行：清理 → 构建 → 格式检查 → 测试 → 代码统计。

```bash
./scripts/ci.sh
```

## 日常开发

```bash
# 清理并重新构建
./scripts/clean.sh && ./scripts/build.sh

# 格式化并检查
./scripts/format.sh && ./scripts/format.sh check

# 完整检查
./scripts/ci.sh
```

## 依赖

- `cmake` 3.28+
- `g++` 或 `clang++`（支持 C++17）
- `clang-format`（可选）
