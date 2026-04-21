#pragma once

#include "entry.hpp"

#include <fstream>
#include <string>
#include <vector>

// 前向声明
namespace x1nglsm {
class KVStore;
}

namespace x1nglsm::core {

/**
 * @brief 预写日志（Write-Ahead Log）
 * @details
 * LSM-Tree的崩溃恢复组件，所有写入操作先记录到WAL再更新MemTable。
 *
 * ## 功能
 * - 持久化：记录所有写入操作，防止崩溃丢失
 * - 恢复：系统重启时从WAL恢复未Flush的数据
 * - 顺序写：追加写模式，保证高性能
 *
 * ## 关键设计
 * - 数据格式：[4字节长度][4字节CRC32][Entry数据]
 * - 刷盘策略：每次Append后sync()（flush + fsync 双重落盘）
 * - CRC32校验：恢复时检测损坏记录，跳过崩溃时写入不完整的数据
 * - 清理时机：MemTable Flush到SSTable后清空
 */
class WriteAheadLog {
  // KVStore 友元类
  friend class x1nglsm::KVStore;

public:
  explicit WriteAheadLog(std::string file_path);

  WriteAheadLog(const WriteAheadLog &) = delete;

  WriteAheadLog &operator=(const WriteAheadLog &) = delete;

  ~WriteAheadLog() { close(); };

  // ========== 写操作 ==========

  // 追加一个Entry到WAL
  bool append(const Entry &entry);

  // 追加多个Entry到WAL
  bool append(const std::vector<Entry> &entries);

  // ========== 读操作 ==========

  // 读取所有Entry（用于崩溃恢复）
  std::vector<Entry> read_all();

private:
  // 核心写入逻辑（不刷盘）
  bool do_append(const Entry &entry);

  // 同步数据到磁盘
  bool sync();

  // 清空WAL文件（Flush完成后调用）
  void clear();

  // 获取当前WAL文件大小
  size_t size() const { return current_size_; };

  // 关闭WAL文件
  void close();

  // WAL 文件路径
  std::string file_path_;
  // 写文件流
  std::ofstream file_;
  // 当前 WAL 文件大小（字节）
  size_t current_size_;
};

} // namespace x1nglsm::core