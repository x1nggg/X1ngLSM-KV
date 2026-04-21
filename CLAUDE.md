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
- `x1nglsm::utils` — 工具函数：`glob_match`、`to_upper`、`format_size`、`crc32` 等

### 核心数据流

所有操作的基本数据单元是 `Entry`（`include/x1nglsm/core/entry.hpp`），贯穿 WAL、MemTable、SSTable 三个组件。Entry 包含 key、value、OpType（PUT/DELETE）、timestamp。

**写入流程**（`KVStore::write_to_wal_and_memtable`）：
1. 先写 WAL（`WriteAheadLog::append` + `flush` 刷盘），保证持久性
2. 再写 MemTable（`MemTable::put` 或 `MemTable::remove`）
3. 每次写入后检查 MemTable 序列化大小，达到 32MB 阈值时触发 `maybe_flush()`

**读取流程**（`KVStore::get`）：
1. 查活跃 MemTable 内部 `table_`（`SkipList`），key 存在时：若为 DELETE 墓碑立即返回 `nullopt`，若为 PUT 返回值
2. 活跃 MemTable 中无该 key 时，查 Immutable MemTable（如果存在），同样处理墓碑
3. Immutable MemTable 中也无该 key 时，遍历 SSTable 列表（从新到旧），先通过 Bloom Filter 预检查（`may_contain`）快速跳过不可能包含该 key 的 SSTable，通过后二分查找 key，遇到 DELETE 墓碑立即短路返回 `nullopt`
4. 全部未找到则返回 `nullopt`

**Flush 流程**（`KVStore::maybe_flush`）：
1. 如果已有 Immutable MemTable（上一轮还没 flush 完），先调用 `flush_immutable()` 将其写入 SSTable
2. 将当前活跃 MemTable move 给 Immutable MemTable（`std::unique_ptr` 所有权转移，O(1)）
3. 创建新的空 MemTable 继续接收写入（写入路径不阻塞），并通过 `advance_timestamp()` 传递时间戳
4. 调用 `flush_immutable()` 将 Immutable MemTable 写入 Level 0 SSTable，然后清空 Immutable MemTable 和 WAL，最后检查是否需要触发 Compaction

**Compaction 流程**（`KVStore::compact`）：
- 采用 Size-Tiered 策略：Level N 的 SSTable 数量达到阈值（4 个）时，合并到 Level N+1
- 合并算法：收集同层所有 SSTable 的 Entry，按 key 升序 + 时间戳降序排序，去重保留最新版本
- 墓碑处理：最底层（下一层及更深层均无数据）丢弃墓碑和被墓碑覆盖的旧 Entry；非最底层保留墓碑
- 合并后删除旧 SSTable 文件，新 SSTable 写入下一层

**崩溃恢复流程**（`KVStore` 构造函数）：
1. `recover_sstables()`：扫描各层目录（`level_0/` ~ `level_N/`），加载已有 SSTable 文件
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
- 追加写入，文件格式为 `[4字节长度][4字节CRC32][Entry序列化数据]` 循环
- 每次 `append()` 后立即 `sync()` 刷盘（`file_.flush()` + `fsync()` 保证数据落盘）
- CRC32 校验：恢复时检测损坏记录（崩溃时写入不完整的数据），跳过损坏部分
- `clear()` 截断文件并重新打开
- 跨平台支持：Windows 用 `_commit()`，Linux/macOS 用 `fsync()`

**SSTable**（`src/core/sstable.cpp`）：
- 文件布局（v3）：压缩数据区 → 索引区 → Bloom Filter 区 → Footer（固定大小）
- 数据区使用 LZ4 压缩，格式为 `[原始大小(4字节)][压缩大小(4字节)][压缩数据]`
- 索引区包含 `IndexEntry`（key + 偏移量 + OpType），支持二分查找，偏移量指向未压缩数据缓冲区（写入时记录，读取时用于定位解压后数据）
- Bloom Filter 区：`[bf_size(4字节)][bf_data]`，存储序列化后的 Bloom Filter，用于查询前预检查
- Footer 包含 magic `"SST\0"`、条目数、数据区结束偏移、版本号（v3）、CRC32 校验和、Bloom Filter 区起始偏移（`reserved` 字段）
- 版本兼容：v3=LZ4 压缩数据区，v2=增加 Bloom Filter，v1=初始格式；v1/v2 文件仍可正常读取
- `load_index()` 惰性加载索引区和 Bloom Filter，`load_data()` 惰性加载并解压数据区，结果缓存在 `mutable` 成员中

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
├── level_0/
│   └── 1.sst                  # SSTable 文件（按 ID 递增编号）
├── level_1/
│   └── 5.sst                  # Compaction 合并后的 SSTable
├── level_2/
└── level_3/
```

## 构建系统说明

- **CMake 3.28+**，C/C++17（C 用于 LZ4 库编译）
- 第三方库 LZ4 以源码形式内嵌在 `third_party/lz4/`，编译为静态库链接
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
| Flush 阈值   | 默认 32 MB（可配置）                                                   | 构造函数参数 `flush_threshold_`，MemTable 序列化大小达到此值触发               |
| SSTable 编号 | 全局递增 uint64_t                                                      | `next_sst_id_` 控制                                                            |
| 时间戳       | 全局递增 uint64_t                                                      | `MemTable::next_timestamp_` 从 1 开始，WAL 恢复后通过 `advance_timestamp` 校正 |
| 依赖         | CMake 3.28+，C++17 编译器（GCC 13+ / Clang 16+），clang-format（可选），LZ4（内嵌 third_party/lz4） |
