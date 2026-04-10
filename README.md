# X1ngLSM-KV

基于 LSM-Tree 架构的轻量级键值存储引擎，使用 C++17 实现。

## 架构概览

```
写入流程: Client → WAL (持久化) → MemTable (内存) → SSTable (磁盘)
读取流程: Client → MemTable → SSTable (从新到旧)
```

核心组件：

| 组件            | 说明                                                                        |
| --------------- | --------------------------------------------------------------------------- |
| **MemTable**    | 内存表，使用手写跳表（SkipList）存储，O(log n) 读写，达到 32MB 时自动 Flush |
| **WAL**         | 预写日志，追加写入 + fsync 落盘 + CRC32 校验，保证崩溃恢复                  |
| **SSTable**     | 磁盘有序表，带索引区、Bloom Filter 和 Footer，支持二分查找                  |
| **BloomFilter** | 布隆过滤器，SSTable 查询前预检查，减少不必要的磁盘 IO                       |
| **Entry**       | 核心数据单元，支持 PUT/DELETE 操作类型，含序列化和反序列化                  |
| **SkipList**    | 手写跳表，MemTable 底层存储，概率平衡 O(log n)                              |

## 项目结构

```
X1ngLSM-KV/
├── include/x1nglsm/       # 头文件
│   ├── kv_store.hpp        # KVStore 主接口
│   ├── core/               # 核心组件
│   │   ├── entry.hpp       # 数据单元
│   │   ├── mem_table.hpp   # 内存表
│   │   ├── sstable.hpp     # 磁盘有序表
│   │   ├── bloom_filter.hpp # 布隆过滤器
│   │   ├── skip_list.hpp    # 跳表
│   │   └── write_ahead_log.hpp  # 预写日志
│   ├── cli/                # CLI 命令
│   │   └── commands.hpp
│   └── utils/              # 工具函数
│       ├── glob_utils.hpp  # 通配符匹配
│       ├── string_utils.hpp
│       ├── arg_utils.hpp
│       ├── system_utils.hpp
│       └── crc32.hpp        # CRC32 校验
├── src/                    # 源文件（结构与 include 对应）
├── test/                   # 测试
├── examples/               # 示例程序
├── scripts/                # 构建/开发脚本
└── CMakeLists.txt
```

## 快速开始

### 构建

```bash
./scripts/build.sh              # Release 构建
./scripts/build.sh debug        # Debug 构建
```

### 运行 CLI

```bash
./bin/cli/x1nglsm-cli           # 使用默认数据目录
./bin/cli/x1nglsm-cli /path/to/db  # 指定数据目录
```

### CLI 命令

| 命令                    | 说明                                 |
| ----------------------- | ------------------------------------ |
| `put <key> <value>`     | 写入                                 |
| `get <key>`             | 查询                                 |
| `del <key>`             | 删除                                 |
| `mput`                  | 批量写入                             |
| `mget`                  | 批量查询                             |
| `mdel`                  | 批量删除                             |
| `keys`                  | 列出所有 key                         |
| `exists <key>`          | 检查 key 是否存在                    |
| `info`                  | 数据库状态信息                       |
| `strlen <key>`          | 获取 value 长度                      |
| `append <key> <value>`  | 追加值                               |
| `setnx <key> <value>`   | 仅当 key 不存在时写入                |
| `incr <key>`            | 自增（key 不存在时初始化为 0）       |
| `decr <key>`            | 自减（key 不存在时初始化为 0）       |
| `incrby <key> <n>`      | 增加指定值（key 不存在时初始化为 0） |
| `decrby <key> <n>`      | 减少指定值（key 不存在时初始化为 0） |
| `getset <key> <value>`  | 设置新值并返回旧值                   |
| `rename <key> <newkey>` | 重命名 key                           |
| `ping`                  | 测试连接                             |
| `flushdb`               | 清空数据库                           |
| `help`                  | 帮助信息                             |

### 运行测试

```bash
./bin/test/test_entry
./bin/test/test_memtable
./bin/test/test_wal
./bin/test/test_kv_store
./bin/test/test_bloom_filter
./bin/test/test_skip_list
```

### 运行示例

```bash
./bin/examples/example_basic        # 基础操作
./bin/examples/example_batch        # 批量操作
./bin/examples/example_persistence  # 持久化和恢复
./bin/examples/example_wildcard     # 通配符查询
```

## KVStore API

```cpp
#include <x1nglsm/kv_store.hpp>

x1nglsm::KVStore store("./data");

// 写入
store.put("key", "value");

// 查询
auto val = store.get("key");  // std::optional<std::string>

// 删除
store.remove("key");

// 批量操作
store.put({{"k1", "v1"}, {"k2", "v2"}});
auto vals = store.get({"k1", "k2"});

// 状态查询
store.exists("key");
store.keys();
store.size();
store.mem_usage();
store.sstables_count();

// 清空
store.clear();
```

## 开发脚本

详见 [scripts/README.md](scripts/README.md)。

```bash
./scripts/build.sh              # 构建
./scripts/clean.sh              # 清理构建目录
./scripts/clean.sh all          # 清理所有（包括数据）
./scripts/format.sh             # 格式化代码
./scripts/format.sh check       # 检查格式
./scripts/ci.sh                 # 完整 CI 流程
```

## 与同类项目对比

|                    | X1ngLSM-KV               | LevelDB       | RocksDB                   |
| ------------------ | ------------------------ | ------------- | ------------------------- |
| 定位               | C++ 初学者进阶项目       | 通用嵌入式 KV | 高性能生产级 KV           |
| 设计原则           | 单线程，无锁，初学者友好 | 单线程安全    | 多线程并发                |
| 外部依赖           | 无                       | 无            | 可选 (lz4/zstd/gflags...) |
| MemTable 底层      | 手写跳表                | 跳表          | 跳表                      |
| 墓碑机制           | 已实现                   | 已实现        | 已实现                    |
| Compaction         | 规划中                   | Level/Sparse  | Level/FIFO/Universal      |
| Bloom Filter       | 已实现                   | 支持          | 支持 (可配置)             |
| Immutable MemTable | 规划中                   | 支持          | 支持                      |
| 数据压缩           | 规划中                   | 支持 (Snappy) | 支持 (多种算法)           |
| WAL 截断           | 已实现                   | 支持          | 支持                      |
| 语言标准           | C++17                    | C++11         | C++17                     |

X1ngLSM-KV 是一个面向 C++ 初学者的 LSM-Tree 存储引擎进阶项目。核心逻辑全部手写，全程单线程无锁设计，代码量小、结构清晰，适合理解 LSM-Tree 的核心原理。

当前处于**阶段2**，已实现 WAL、MemTable（手写跳表）、SSTable、Bloom Filter、墓碑机制与崩溃恢复。后续计划引入 Compaction、数据压缩、Immutable MemTable 等优化，最终目标对标 LevelDB 基础设计。

生产环境请选用 [LevelDB](https://github.com/google/leveldb) 或 [RocksDB](https://github.com/facebook/rocksdb)。

## 依赖

- CMake 3.28+
- C++17 编译器 (GCC 13+ / Clang 16+)
- clang-format (可选，代码格式化)

## 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install cmake g++ clang-format
```
