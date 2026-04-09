# 架构设计

## 整体架构

X1ngLSM-KV 采用经典的 LSM-Tree（Log-Structured Merge-Tree）架构，数据按写入顺序组织，通过分层存储实现高吞吐写入。

```
┌─────────────────────────────────────────────┐
│                  Client                      │
│              (CLI / C++ API)                 │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│                KVStore                       │
│            (对外统一接口)                     │
└───┬──────────┬──────────┬───────────────────┘
    │          │          │
    ▼          ▼          ▼
┌───────┐ ┌───────┐ ┌──────────┐
│  WAL  │ │MemTable│ │ SSTable  │
│(预写日志)│ │(内存表) │ │(磁盘有序表)│
└───────┘ └───┬───┘ └──────────┘
    │         │          ▲
    │    32MB 阈值       │
    │         │          │
    │         └──Flush───┘
    │
    └── MemTable Flush 后清空
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
- 达到 **32MB** 时触发 Flush
- 删除操作写入墓碑（Tombstone）Entry，不实际删除

### SkipList — 跳表

- 概率平衡的有序数据结构，header-only 模板类
- MAX_LEVEL = 16，每层 50% 晋升概率
- 支持 `insert`、`find`、`clear`、`size`、`empty`
- 遍历通过 `header()->forward[0]` 链表实现

### WAL — 预写日志

- 追加写入模式（Append-Only）
- 每次 `append()` 后调用 `flush()` 刷盘
- MemTable Flush 到 SSTable 后清空 WAL
- 文件格式：`[4字节长度][Entry数据]` 循环

### SSTable — 磁盘有序表

文件布局（v2）：

```
┌──────────────────────────────────┐
│           数据区                  │
│  [Entry1][Entry2]...[EntryN]     │
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

- **数据区**：按 key 有序存储的 Entry 列表
- **索引区**：每条记录 key 和对应的数据区偏移量，支持二分查找
- **Bloom Filter 区**：序列化后的 Bloom Filter，查询前预检查，跳过不存在的 key
- **Footer**：文件校验信息，含 magic `"SST\0"`、条目数、版本号（v2）、Bloom Filter 区偏移（`reserved` 字段）等
- v1 文件（无 Bloom Filter）仍可正常读取

读取流程：加载 Footer → 定位索引区和 Bloom Filter → Bloom Filter 预检查 → 二分查找 key → 按偏移量读取数据。

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
           │       └─ flush to disk
           │
           └─→ MemTable::put(entry)    // 2. 再写 MemTable（内存）
                   │
                   └─ if size >= 32MB
                       └─→ maybe_flush()
                             ├─ SSTable::write_from_entries()
                             ├─ WAL::clear()
                             └─ MemTable::clear()
```

### 读取流程

```
Client → KVStore::get(key)
           │
           ├─→ MemTable 查找          // 1. 先查内存
           │     ├─ key 存在且为 PUT → 返回值
           │     └─ key 存在且为 DELETE → 返回 nullopt（墓碑短路）
           │
           └─→ SSTable 查找（从新到旧） // 2. 逐表查找
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
    ├─→ recover_sstables()         // 1. 加载已有 SSTable 文件
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
- 墓碑数据在 Flush 时写入 SSTable，未来可通过 Compaction 清理

## 数据目录结构

```
data/
├── wal.log                    # WAL 文件
└── sstables/
    ├── 1.sst               # SSTable 文件（按 ID 递增编号）
    ├── 2.sst
    └── ...
```

## 全局设计约束

| 约束         | 值                                                              | 说明                                  |
| ------------ | --------------------------------------------------------------- | ------------------------------------- |
| Flush 阈值   | 32 MB                                                           | MemTable 序列化大小达到此值触发 Flush |
| SSTable 编号 | 全局递增 uint64_t                                               | `next_sst_id_` 控制                   |
| 时间戳       | 全局递增 uint64_t                                               | MemTable 内维护 `next_timestamp_`     |
| 命名空间     | `x1nglsm` / `x1nglsm::core` / `x1nglsm::cli` / `x1nglsm::utils` | —                                     |
| C++ 标准     | C++17                                                           | —                                     |
