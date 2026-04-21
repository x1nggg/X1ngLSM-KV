#pragma once

#include "x1nglsm/core/bloom_filter.hpp"
#include "x1nglsm/core/entry.hpp"
#include "x1nglsm/utils/crc32.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace x1nglsm::core {

// 索引项：记录 key 和对应的数据偏移量（v3 指向解压缓冲区，v1/v2 指向文件偏移）
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
  // 压缩数据区结束位置（即索引区起始位置）
  uint64_t data_end_offset;
  // 版本号：v1=初始格式，v2=增加 Bloom Filter，v3=数据区 LZ4 压缩
  uint32_t version;
  // CRC32 校验和，对压缩数据区计算，用于检测文件损坏
  uint32_t checksum;
  // 预留字段，目前用于记录 Bloom Filter 区的起始偏移，未来可以扩展其他元数据
  uint32_t reserved;
};

class SSTable {
public:
  explicit SSTable(std::string file_path)
      : file_path_(std::move(file_path)), bloom_filter_() {}

  ~SSTable() = default;

  // 写入：从 Entry 列表创建 SSTable 文件
  bool write_from_entries(const std::vector<Entry> &entries);

  // 读取：根据 key 查找 value
  std::optional<std::string> get(const std::string &key) const;

  // 根据已知偏移量直接读取 Entry 的 value（跳过索引查找和 Bloom Filter）
  std::optional<std::string> get_value_at(size_t offset) const;

  // Bloom Filter 预检查：key 是否可能存在（轻量，不加载索引）
  bool may_contain(const std::string &key) const;

  // 获取所有存活 key（非墓碑）
  std::vector<std::string> keys() const;

  // 获取所有 key（包括墓碑）
  std::vector<std::string> all_keys() const;

  // 获取所有 Entry（用于 Compaction）
  std::vector<Entry> get_all_entries() const;

  // 获取索引项（包含类型信息，用于跨 SSTable 墓碑处理）
  const std::vector<IndexEntry> &index_entries() const;

  // 获取文件信息
  const std::string &file_path() const { return file_path_; };

  size_t num_entries() const { return index_.size(); }

private:
  // 惰性加载索引区（Bloom Filter 未加载时一并加载）
  bool load_index() const;

  // 惰性加载 Bloom Filter（不加载索引，用于轻量预检查）
  bool load_bloom_filter() const;

  // 惰性加载并缓存 Footer（只读一次）
  bool load_footer() const;

  // 惰性加载并解压数据区（仅 v3）
  bool load_data() const;

  // 计算数据的 CRC32 校验和
  uint32_t compute_checksum(const std::string &data) const {
    return utils::crc32(data);
  }

  // SSTable 文件路径
  std::string file_path_;
  // 索引项缓存
  mutable std::vector<IndexEntry> index_;
  // Bloom Filter 缓存
  mutable BloomFilter bloom_filter_;
  // Bloom Filter 是否已加载
  mutable bool bloom_filter_loaded_ = false;
  // 解压后的数据区缓存
  mutable std::string decompressed_data_;
  // 数据区是否已加载并解压
  mutable bool data_loaded_ = false;
  // Footer 缓存（惰性加载，只读一次）
  mutable SSTableFooter footer_{};
  // Footer 是否已加载
  mutable bool footer_loaded_ = false;
};

} // namespace x1nglsm::core