# 架构设计

## 整体架构

X1ngLSM-KV 采用经典的 LSM-Tree（Log-Structured Merge-Tree）架构，数据按写入顺序组织，通过分层存储实现高吞吐写入。

```
┌──────────────────────────────────────────────────────────────┐
│                         Client                               │
│                     (CLI / C++ API)                          │
└─────────────────────────┬────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────────┐
│                        KVStore                               │
│                    (对外统一接口)                              │
└───┬──────────────┬──────────────┬────────────────────────────┘
    │              │              │
    ▼              ▼              ▼
┌───────┐  ┌─────────────┐  ┌──────────┐
│  WAL  │  │  MemTable   │  │ SSTable  │
│(预写日志)│  │ (活跃，可写)│  │(磁盘有序表)│
└───────┘  └──────┬──────┘  └──────────┘
    │              │               ▲
    │         32MB 阈值             │
    │              │               │
    │              ▼               │
    │     ┌────────────────┐       │
    │     │    Immutable   │       │
    │     │    MemTable    │       │
    │     │   (只读，等待    │───────┘
    │     │    flush)      │ flush_immutable()
    │     └────────────────┘
    │              │
    └── flush 完成后清空 WAL
```

## 核心组件

### Entry — 数据单元

系统中所有操作的基本单位，贯穿 WAL、MemTable、SSTable。

```cpp
struct Entry {
    std::string key;       // 键
    std::string value;     // 值（DELETE 时为空）
    OpType type;           // PUT 或 DELETE
    uint64_t timestamp;    // 全局递增时间戳
};
```

序列化格式（二进制，小端序）：

```
[type(1字节)][timestamp(8字节)][key长度(4字节)][key数据][value长度(4字节)][value数据]
```

### MemTable — 内存表

- 底层使用手写跳表（`SkipList`），数据按 key 有序，期望 O(log n) 查找/插入
- 维护 `total_encoded_size_` 追踪序列化后大小
- 达到 **Flush 阈值（默认 32MB）** 时触发 Flush
- 删除操作写入墓碑（Tombstone）Entry，不实际删除

### SkipList — 跳表

- 概率平衡的有序数据结构，header-only 模板类
- MAX_LEVEL = 16，每层 50% 晋升概率
- 支持 `insert`、`find`、`clear`、`size`、`empty`
- 遍历通过 `header()->forward[0]` 链表实现

### WAL — 预写日志

- 追加写入模式（Append-Only）
- 每次 `append()` 后调用 `sync()` 刷盘（`file_.flush()` + `fsync()` 双重保证数据落盘）
- MemTable Flush 到 SSTable 后清空 WAL
- 文件格式：`[4字节长度][4字节CRC32][Entry数据]` 循环
- CRC32 校验：恢复时检测损坏记录，跳过崩溃时写入不完整的数据
- 跨平台 fsync：Windows 用 `_commit()`，Linux/macOS 用 `fsync()`

### SSTable — 磁盘有序表

文件布局（v3）：

```
┌──────────────────────────────────┐
│        压缩数据区 (LZ4)           │
│  [原始大小(4)][压缩大小(4)]       │
│  [压缩数据]                      │
├──────────────────────────────────┤
│           索引区                  │
│  [IndexEntry1]...[IndexEntryN]   │
├──────────────────────────────────┤
│        Bloom Filter 区           │
│  [bf_size(4)][bf_data]           │
├──────────────────────────────────┤
│           Footer (固定大小)       │
│  magic(4) | num_entries(4)       │
│  data_end(8) | version(4)       │
│  checksum(4) | reserved(4)      │
└──────────────────────────────────┘
```

- **压缩数据区**：Entry 列表序列化后用 LZ4 压缩，索引中的偏移量指向未压缩数据缓冲区（写入时记录，读取时用于定位解压后数据）
- **索引区**：每条记录 key 和对应的数据偏移量，支持二分查找
- **Bloom Filter 区**：序列化后的 Bloom Filter，查询前预检查，跳过不存在的 key
- **Footer**：文件校验信息，含 magic `"SST\0"`、条目数、版本号（v3）、CRC32 校验和（对压缩数据区计算，用于检测文件损坏）、Bloom Filter 区偏移（`reserved` 字段）等
- 版本兼容：v3=LZ4 压缩数据区，v2=增加 Bloom Filter，v1=初始格式；v1/v2 文件仍可正常读取

读取流程（三步渐进式加载）：Footer 缓存（只读一次）→ Bloom Filter 预检查（轻量，跳过不相关的 SSTable）→ 索引加载 + 二分查找 → v3 从未压缩数据缓冲区读取 Entry / v1/v2 从文件 seek 读取。所有组件惰性缓存，每个 SSTable 实例只加载一次。

### BloomFilter — 布隆过滤器

- 概率型数据结构，判断 key "可能存在" 或 "一定不存在"
- 每个 SSTable 关联一个 Bloom Filter
- 查询前先检查 Bloom Filter，跳过不可能包含该 key 的 SSTable，减少磁盘 IO
- 使用双哈希法生成多个哈希值，位数组和哈希函数数量根据预期元素数和误判率自动计算

## 关键流程

### 写入流程

```
Client → KVStore::put()
           │
           ├─→ WAL::append(entry)      // 1. 先写 WAL（持久化）
           │       └─ sync to disk
           │
           └─→ MemTable::put(entry)    // 2. 再写活跃 MemTable（内存）
                   │
                   └─ if size >= 32MB
                       └─→ maybe_flush()
                             ├─ flush_immutable()       // 如果有旧的 immutable，先 flush
                             ├─ move memtable → immutable // 所有权转移，O(1)
                             ├─ 创建新的空 MemTable      // 写入路径不阻塞
                             │   └─ advance_timestamp()   // 传递时间戳
                             └─ flush_immutable()        // 将 immutable 写入 Level 0 SSTable
                                   ├─ SSTable::write_from_entries()
                                   ├─ immutable.reset()
                                   ├─ WAL::clear()
                                   └─ maybe_compact()     // 检查是否需要 Compaction
```

### 读取流程

```
Client → KVStore::get(key)
           │
           ├─→ 活跃 MemTable 查找    // 1. 先查活跃内存表
           │     ├─ key 存在且为 PUT → 返回值
           │     └─ key 存在且为 DELETE → 返回 nullopt（墓碑短路）
           │
           ├─→ Immutable MemTable 查找  // 2. 查 Immutable（如果存在）
           │     ├─ key 存在且为 PUT → 返回值
           │     └─ key 存在且为 DELETE → 返回 nullopt（墓碑短路）
           │
           └─→ SSTable 查找（逐层 Level 0 → Level N，每层从新到旧） // 3. 逐表查找
                 ├─ Bloom Filter 预检查 → 不可能包含则跳过
                 ├─ key 存在且为 PUT → 返回值
                 ├─ key 存在且为 DELETE → 返回 nullopt（墓碑短路，不继续搜索旧表）
                 └─ key 不存在 → 继续搜索更旧的表
                 └─ 全部未找到 → nullopt
```

### 崩溃恢复流程

```
KVStore 构造
    │
    ├─→ recover_sstables()         // 1. 扫描各层目录，加载已有 SSTable 文件
    │
    └─→ recover_from_wal()         // 2. 从 WAL 重放未 Flush 的操作
          ├─ WAL::read_all()
          ├─ MemTable::put()       // 逐条回放
          └─ MemTable::advance_timestamp()  // 推进时间戳，避免重复
```

## 删除机制

不直接删除数据，而是写入一条 `OpType::DELETE` 的 Entry（墓碑）。

- 读取时遇到墓碑立即返回 `nullopt`，**不再继续搜索更旧的数据层**
- `MemTable` 中的墓碑会阻止对 SSTable 的搜索
- `SSTable` 中的墓碑会阻止对更旧 SSTable 的搜索
- `KVStore::get()` 通过直接检查 `MemTable` 内部存储和 `SSTable::index_entries()` 的类型信息实现墓碑短路
- `KVStore::keys()` 同样利用类型信息正确过滤跨 SSTable 的墓碑
- 墓碑数据在 Flush 时写入 SSTable，通过 Compaction 合并时清理

## 数据目录结构

```
data/
├── wal.log                    # WAL 文件
├── level_0/
│   └── 1.sst               # SSTable 文件（按 ID 递增编号）
├── level_1/
│   └── 5.sst               # Compaction 合并后的 SSTable
├── level_2/
└── level_3/
```

## Compaction（压缩合并）

采用 Size-Tiered 策略：

- **触发条件**：Level N 的 SSTable 数量达到阈值（`COMPACTION_TRIGGER = 4`）时触发
- **合并流程**：`KVStore::compact(level)` 收集同层所有 SSTable 的 Entry → 按 key 升序 + 时间戳降序排序 → 去重保留最新版本 → 写入新 SSTable 到 Level N+1 → 删除旧文件
- **墓碑处理**：判断 Level N+1 是否为事实上的最底层（更深层均无数据）。最底层丢弃墓碑和被墓碑覆盖的旧 Entry；非最底层保留墓碑
- **全局参数**：`MAX_LEVEL = 4`（最大层数），`COMPACTION_TRIGGER = 4`（触发阈值）
- Flush 完成后自动调用 `maybe_compact()` 检查是否需要触发

## 全局设计约束

| 约束         | 值                                                              | 说明                                                             |
| ------------ | --------------------------------------------------------------- | ---------------------------------------------------------------- |
| Flush 阈值   | 默认 32 MB（可配置）                                            | 构造函数参数 `flush_threshold_`，MemTable 序列化大小达到此值触发 |
| SSTable 编号 | 全局递增 uint64_t                                               | `next_sst_id_` 控制                                              |
| 时间戳       | 全局递增 uint64_t                                               | `MemTable::next_timestamp_` 从 1 开始，跨 MemTable 通过 `advance_timestamp()` 传递 |
| 命名空间     | `x1nglsm` / `x1nglsm::core` / `x1nglsm::cli` / `x1nglsm::utils` | —                                                                |
| C++ 标准     | C++17                                                           | —                                                                |
| 压缩算法     | LZ4（内嵌 third_party/lz4）                                     | SSTable 数据区压缩                                               |
| Compaction   | Size-Tiered 策略                                                | `COMPACTION_TRIGGER = 4`（触发阈值），`MAX_LEVEL = 4`（最大层数）|
