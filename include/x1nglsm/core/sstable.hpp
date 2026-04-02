#pragma once

#include "x1nglsm/core/entry.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>


namespace x1nglsm::core {

// 索引项：记录key和对应的文件偏移量
struct IndexEntry {
  std::string key;
  uint64_t offset;
  OpType type; // 操作类型，用于过滤墓碑
};

// Footer：文件末尾的元数据
struct SSTableFooter {
  // 文件标识，用于验证这是否是合法的 SSTable 文件。固定为 "SST\0"
  char magic[4];
  // 记录文件中有多少个 Entry，用于读取时验证完整性
  uint32_t num_entries;
  // 数据区结束位置
  uint64_t data_end_offset;
  // 版本号，未来文件格式升级时可以兼容旧版本
  uint32_t version;
  // 数据完整性校验，防止文件损坏
  uint32_t checksum;
  // 预留字段，方便未来扩展功能
  uint32_t reserved;
};

class SSTable {
public:
  explicit SSTable(std::string file_path) : file_path_(std::move(file_path)) {}

  ~SSTable() = default;

  // 写入：从 Entry 列表创建 SSTable 文件
  bool write_from_entries(const std::vector<Entry> &entries);

  // 读取：根据 key 查找 value
  std::optional<std::string> get(const std::string &key) const;

  // 获取所有存活 key（非墓碑）
  std::vector<std::string> keys() const;

  // 获取所有 key（包括墓碑）
  std::vector<std::string> all_keys() const;

  // 获取索引项（包含类型信息，用于跨 SSTable 墓碑处理）
  const std::vector<IndexEntry> &index_entries() const;

  // 获取文件信息
  const std::string &file_path() const { return file_path_; };

  size_t num_entries() const { return index_.size(); }

private:
  bool load_index() const;

  uint32_t compute_checksum(const std::string &data) const;

  std::string file_path_;
  mutable std::vector<IndexEntry> index_;
};

} // namespace x1nglsm::core