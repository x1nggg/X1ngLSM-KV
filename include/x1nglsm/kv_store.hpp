#pragma once

#include "x1nglsm/core/mem_table.hpp"
#include "x1nglsm/core/sstable.hpp"
#include "x1nglsm/core/write_ahead_log.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace x1nglsm {

/**
 * @brief 键值存储引擎（Key-Value Store）
 * @details
 * LSM-Tree架构的对外接口，协调 MemTable、WAL 和 SSTable 工作，提供完整的 KV
 * 操作。
 *
 * ## 功能
 * - Put：写入 key-value 对
 * - Get：查询 key 对应的 value
 * - Delete：删除key
 * - Flush：自动将 MemTable 数据持久化到 SSTable
 * - 崩溃恢复：启动时从 SSTable 和 WAL 恢复数据
 *
 * ## 架构组件
 * - MemTable：活跃内存表，使用跳表（SkipList）存储最新数据
 * - Immutable MemTable：只读内存表，等待 Flush 到 SSTable（活跃 MemTable 满时
 * move 而来）
 * - WAL：预写日志，保证数据不丢失
 * - SSTable：磁盘存储表，支持索引快速查找
 *
 * ## 关键设计
 * - 写入流程：先写 WAL（持久化），再写活跃 MemTable（内存）
 * - 读取流程：活跃 MemTable → Immutable MemTable → SSTable（从新到旧）
 * - Flush机制：MemTable 达到32MB时 move 给 Immutable，创建新 MemTable，异步
 * flush Immutable 到 SSTable
 * - 崩溃恢复：启动时从 SSTable 加载已有数据，再从 WAL 重放最新操作
 */
class KVStore {
public:
  // 构造函数：传入数据目录和可选的 flush 阈值（默认 32MB），自动恢复
  explicit KVStore(std::string db_dir,
                   size_t flush_threshold = 32 * 1024 * 1024);

  KVStore(const KVStore &) = delete;

  KVStore &operator=(const KVStore &) = delete;

  ~KVStore() = default;

  // 写入 key-value
  bool put(const std::string &key, const std::string &value);

  // 批量写入 key-value
  bool put(const std::vector<std::pair<std::string, std::string>> &kvs);

  // 查询key
  std::optional<std::string> get(const std::string &key) const;

  // 批量查询 key
  std::vector<std::optional<std::string>>
  get(const std::vector<std::string> &keys) const;

  // 删除key（写入墓碑 key）
  bool remove(const std::string &key);

  // 批量删除 key
  bool remove(const std::vector<std::string> &keys);

  // 检查 key 是否存在
  bool exists(const std::string &key) const;

  // 获取所有 key（包括 MemTable 和 SSTable）
  std::vector<std::string> keys() const;

  // 获取key数量（包括 MemTable 和 SSTable）
  size_t size() const { return keys().size(); }

  // 获取 SSTable 数量
  size_t sstables_count() const { return sstables_.size(); }

  // 获取内存使用（字节）
  size_t mem_usage() const;

  // 获取 WAL 文件大小（字节）
  size_t wal_size() const { return wal_->size(); }

  // 清空数据库（包括 MemTable、WAL 和所有 SSTable）
  void clear();

private:
  // 从 WAL 恢复 MemTable
  void recover_from_wal();

  // 从磁盘恢复已有 SSTable
  void recover_sstables();

  // MemTable 达到阈值时，move 给 Immutable MemTable，然后 flush Immutable 到
  // SSTable
  void maybe_flush();

  // 将 Immutable MemTable 写入 SSTable
  void flush_immutable();

  bool has_immutable() const { return immutable_mem_table_ != nullptr; };

  // 写入 WAL 和 MemTable 的统一逻辑
  bool write_to_wal_and_memtable(const core::Entry &entry);

  // 数据目录
  std::string db_dir_;
  // 活跃内存表（可写）
  std::unique_ptr<core::MemTable> mem_table_;
  // 只读内存表（等待 flush 到 SSTable）
  std::unique_ptr<core::MemTable> immutable_mem_table_;
  // 预写日志
  std::unique_ptr<core::WriteAheadLog> wal_;
  // SSTable 列表（用于查询）
  std::vector<std::unique_ptr<core::SSTable>> sstables_;
  // 下一个 SSTable 文件 ID
  uint64_t next_sst_id_;
  // SSTable 文件目录
  std::string sstable_dir_;
  // Flush 阈值（字节）
  size_t flush_threshold_;
};

} // namespace x1nglsm