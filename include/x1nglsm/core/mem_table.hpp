#pragma once

#include "entry.hpp"
#include "x1nglsm/core/skip_list.hpp"

#include <optional>
#include <string>
#include <vector>

// 前向声明
namespace x1nglsm {
class KVStore;
}

namespace x1nglsm::core {

/**
 * @brief 内存表（Memory Table）
 * @details
 * LSM-Tree架构的内存层组件，负责缓存最新数据并支持快速读写。
 *
 * ## 功能
 * - 写入缓冲：减少磁盘IO，批量Flush到SSTable
 * - 快速查询：O(log n)的读写性能
 * - 版本管理：通过时间戳区分新旧数据
 * - 删除支持：使用墓碑（Tombstone）标记删除
 *
 * ## 关键设计
 * - 使用跳表（SkipList）存储，数据按key有序
 * - 时间戳全局递增，保证数据新旧关系
 * - 删除不真正删除数据，而是写入DELETE类型Entry
 * - 维护序列化大小，用于判断Flush时机（默认32MB）
 */
class MemTable {
  // 测试友元类
  friend class MemTableTest;
  // KVStore 友元类
  friend class x1nglsm::KVStore;

public:
  MemTable();

  ~MemTable() = default;

  // ========== 写操作 ==========

  /**
   * @brief 插入或更新key-value
   * @details 如果key已经存在，覆盖旧值（用新的timestamp）
   * @param key
   * @param value
   * @param timestamp 可选的时间戳，不提供则自动生成
   */
  void put(const std::string &key, const std::string &value,
           std::optional<uint64_t> timestamp = std::nullopt);

  // 删除key（写入墓碑Entry）
  void remove(const std::string &key,
              std::optional<uint64_t> timestamp = std::nullopt);

  // ========== 读操作 ==========

  /**
   * @brief 查询key
   * @param key
   * @return
   *  - 有值：key存在且是PUT操作
   *  - nullopt：key不存在，或key存在但是DELETE墓碑
   */
  std::optional<std::string> get(const std::string &key) const;

  // ========== 状态查询 ==========

  // 获取当前Entry数量（包括墓碑key）
  size_t num_entries() const { return table_.size(); };

  // 获取当前有效key数量（不包括墓碑key）
  size_t size() const;

  // 获取所有有效key（不包括墓碑key）
  std::vector<std::string> keys() const;

  // 判断是否为空
  bool empty() const { return table_.empty(); };

  size_t total_encoded_size() const { return total_encoded_size_; };

  // 获取下一个时间戳
  uint64_t get_next_timestamp() { return next_timestamp_++; };

  // 将 next_timestamp_ 推进到至少 ts（用于 WAL 恢复后校正时间戳）
  void advance_timestamp(uint64_t ts) {
    if (ts > next_timestamp_)
      next_timestamp_ = ts;
  }

private:
  // 清空MemTable
  void clear() {
    table_.clear();
    total_encoded_size_ = 0;
  };

  // 获取所有Entry（按key有序，用于Flush）
  std::vector<Entry> get_all_entries() const;

  // 核心存储：跳表，数据按 key 有序
  SkipList<std::string, Entry> table_;
  // 全局时间戳计数器，每次写入递增
  uint64_t next_timestamp_;
  // 记录当前MemTable的序列化字节大小，用于判断何时触发Flush
  size_t total_encoded_size_;
};

} // namespace x1nglsm::core
