# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

X1ngLSM-KV 是一个基于 LSM-Tree（Log-Structured Merge-Tree）架构的轻量级键值存储引擎，使用 C++17 实现。核心写入路径为 `Client → WAL（持久化）→ MemTable（内存）→ SSTable（磁盘）`，读取路径为 `Client → MemTable → SSTable（从新到旧）`。

## 常用命令

### 构建

```bash
./scripts/build.sh              # Release 构建
./scripts/build.sh debug        # Debug 构建
./scripts/build.sh clean        # 清理构建目录
./scripts/build.sh test         # 构建并运行测试
```

或手动构建：
```bash
cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
```

### 运行测试

```bash
# 逐个运行测试（输出在 bin/test/ 目录）
./bin/test/test_entry
./bin/test/test_memtable
./bin/test/test_wal
./bin/test/test_kv_store
./bin/test/test_bloom_filter
./bin/test/test_skip_list
```

注意：测试二进制输出到 `bin/test/`，不是 `build/test/`。测试使用自定义框架（布尔返回值），不依赖 gtest。

### 代码格式化

```bash
./scripts/format.sh             # 格式化代码
./scripts/format.sh check       # 仅检查格式（CI 用）
```

### 完整 CI 流程

```bash
./scripts/ci.sh                 # 清理 → 构建 → 格式检查 → 测试 → 代码统计
```

### 运行 CLI

```bash
./bin/cli/x1nglsm-cli           # 默认数据目录 ./data/cli
./bin/cli/x1nglsm-cli --dir /path/to/db  # 指定数据目录
```

## 架构设计

### 命名空间

- `x1nglsm` — 顶层命名空间，包含 `KVStore` 主接口
- `x1nglsm::core` — 核心组件：`Entry`、`MemTable`、`SSTable`、`WriteAheadLog`、`BloomFilter`、`SkipList`
- `x1nglsm::cli` — CLI 命令处理
- `x1nglsm::utils` — 工具函数：`glob_match`、`to_upper`、`format_size` 等

### 核心数据流

所有操作的基本数据单元是 `Entry`（`include/x1nglsm/core/entry.hpp`），贯穿 WAL、MemTable、SSTable 三个组件。Entry 包含 key、value、OpType（PUT/DELETE）、timestamp。

**写入流程**（`KVStore::write_to_wal_and_memtable`）：
1. 先写 WAL（`WriteAheadLog::append` + `flush` 刷盘），保证持久性
2. 再写 MemTable（`MemTable::put` 或 `MemTable::remove`）
3. 每次写入后检查 MemTable 序列化大小，达到 32MB 阈值时触发 `maybe_flush()`

**读取流程**（`KVStore::get`）：
1. 查 MemTable 内部 `table_`（`SkipList`），key 存在时：若为 DELETE 墓碑立即返回 `nullopt`，若为 PUT 返回值
2. MemTable 中无该 key 时，遍历 SSTable 列表（从新到旧），先通过 Bloom Filter 预检查（`may_contain`）快速跳过不可能包含该 key 的 SSTable，通过后二分查找 key，遇到 DELETE 墓碑立即短路返回 `nullopt`
3. 全部未找到则返回 `nullopt`

**Flush 流程**（`KVStore::maybe_flush`）：
1. 调用 `SSTable::write_from_entries()` 将 MemTable 所有 Entry 写入磁盘
2. 调用 `WAL::clear()` 清空 WAL
3. 调用 `MemTable::clear()` 清空内存表

**崩溃恢复流程**（`KVStore` 构造函数）：
1. `recover_sstables()`：扫描 `sstables/` 目录，加载已有 SSTable 文件
2. `recover_from_wal()`：从 WAL 重放未 Flush 的操作到 MemTable，然后调用 `MemTable::advance_timestamp(max_ts + 1)` 推进时间戳

### 关键组件

**MemTable**（`src/core/mem_table.cpp`）：
- 底层 `SkipList<std::string, Entry>`，按 key 有序，期望 O(log n) 查找/插入
- 维护 `total_encoded_size_` 追踪序列化大小，用于 flush 判断
- `next_timestamp_` 从 1 递增，保证全局唯一
- `advance_timestamp()` 用于 WAL 恢复后校正时间戳（取 max）

**SkipList**（`include/x1nglsm/core/skip_list.hpp`）：
- 概率平衡的有序数据结构，header-only 模板类
- MAX_LEVEL = 16，每层 50% 晋升概率
- 支持 `insert`、`find`、`clear`、`size`、`empty`，遍历通过 `header()->forward[0]` 链表

**WAL**（`src/core/write_ahead_log.cpp`）：
- 追加写入，文件格式为 `[4字节长度][Entry序列化数据]` 循环
- 每次 `append()` 后立即 `flush()` 刷盘
- `clear()` 截断文件并重新打开

**SSTable**（`src/core/sstable.cpp`）：
- 文件布局（v2）：数据区 → 索引区 → Bloom Filter 区 → Footer（固定大小）
- 索引区包含 `IndexEntry`（key + 偏移量 + OpType），支持二分查找
- Bloom Filter 区：`[bf_size(4字节)][bf_data]`，存储序列化后的 Bloom Filter，用于查询前预检查
- Footer 包含 magic `"SST\0"`、条目数、数据区结束偏移、版本号（v2）、Bloom Filter 区起始偏移（`reserved` 字段）
- v1 文件（无 Bloom Filter）仍可正常读取，默认 Bloom Filter 对所有 key 返回 true
- `index_entries()` 惰性加载（首次访问时从磁盘读取索引区和 Bloom Filter），结果缓存在 `mutable` 成员中

**Entry 序列化格式**（小端序）：
```
[type(1字节)][timestamp(8字节)][key长度(4字节)][key][value长度(4字节)][value]
```

### 删除机制（墓碑）

删除操作不直接删除数据，而是写入 `OpType::DELETE` 的 Entry（墓碑）。读取时遇到墓碑立即短路返回 `nullopt`，不会继续搜索更旧的数据层。墓碑在 `KVStore::get()`、`KVStore::keys()` 中均被正确处理。

### 数据目录结构

```
data/
├── wal.log                    # WAL 文件
└── sstables/
    ├── sstable_000001.sst     # SSTable 文件（按创建顺序编号）
    └── ...
```

## 构建系统说明

- **CMake 3.28+**，C++17
- 测试目标直接链接所需源文件（不链接库），每个测试在 `test/CMakeLists.txt` 中独立声明
- 可执行文件输出目录：CLI → `bin/cli/`，测试 → `bin/test/`，示例 → `bin/examples/`
- 头文件在 `include/` 下，与 `src/` 目录结构对应
- KVStore 是 MemTable 的友元类（直接访问 `table_`），测试中使用 `MemTableTest` 友元类访问私有成员

## 测试约定

- 每个测试函数返回 `bool`（`true` = 通过）
- 使用手动注册的测试数组 + 循环运行，输出 `[PASS]`/`[FAIL]`
- 测试用 KVStore 数据目录在 `./data/test/` 下，每个测试用例独立目录，用完清理
- WAL 测试文件直接在当前目录创建 `.log` 文件，用完清理

## 全局设计约束

| 约束         | 值                                                                     | 说明                                                                           |
| ------------ | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| Flush 阈值   | 32 MB                                                                  | `static constexpr size_t THRESHOLD`，MemTable 序列化大小达到此值触发           |
| SSTable 编号 | 全局递增 uint64_t                                                      | `next_sst_id_` 控制                                                            |
| 时间戳       | 全局递增 uint64_t                                                      | `MemTable::next_timestamp_` 从 1 开始，WAL 恢复后通过 `advance_timestamp` 校正 |
| 依赖         | CMake 3.28+，C++17 编译器（GCC 13+ / Clang 16+），clang-format（可选） |
