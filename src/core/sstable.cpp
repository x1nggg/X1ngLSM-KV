#include "x1nglsm/core/sstable.hpp"
#include "x1nglsm/core/bloom_filter.hpp"

#include "lz4.h"
#include "x1nglsm/kv_store.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

namespace x1nglsm::core {

// 从 Entry 列表创建 SSTable 文件（v3 格式）
// 文件布局：[压缩数据区] → [索引区] → [Bloom Filter 区] → [Footer]
// 压缩数据区格式：[原始大小(4)][压缩大小(4)][LZ4 压缩数据]
// 索引区格式：每条 [key_len(4)][key][offset(8)][type(1)]，offset
// 指向未压缩数据缓冲区（写入时记录，读取时用于定位解压后数据）
bool SSTable::write_from_entries(const std::vector<Entry> &entries) {
  std::ofstream file(file_path_, std::ios::binary);
  if (!file)
    return false;

  // 1.序列化所有 Entry 到内存缓冲区
  std::vector<uint64_t> offsets; // 记录每个 Entry 在缓冲区内的偏移量
  std::string data_buffer;
  for (const auto &entry : entries) {
    uint64_t offset = data_buffer.size();
    offsets.emplace_back(offset);

    std::string data = entry.encode();
    auto len = static_cast<uint32_t>(data.size());

    data_buffer.append(reinterpret_cast<const char *>(&len), sizeof(len));
    data_buffer.append(data);
  }

  // 2. LZ4 压缩
  int max_size = LZ4_compressBound(static_cast<int>(data_buffer.size()));
  std::string compressed(max_size, '\0');
  int compressed_size =
      LZ4_compress_default(data_buffer.data(), compressed.data(),
                           static_cast<int>(data_buffer.size()), max_size);
  compressed.resize(compressed_size);

  // 3. 写入压缩数据区
  auto uncompressed_size = static_cast<uint32_t>(data_buffer.size());
  auto comp_size = static_cast<uint32_t>(compressed_size);
  file.write(reinterpret_cast<const char *>(&uncompressed_size),
             sizeof(uncompressed_size));
  file.write(reinterpret_cast<const char *>(&comp_size), sizeof(comp_size));
  file.write(compressed.data(), compressed_size);

  // 对压缩数据计算 checksum
  uint32_t checksum = compute_checksum(compressed);

  // 记录数据区结束位置（即索引区开始位置）
  std::streampos data_end = file.tellp();

  // 4.写入索引区
  // 格式：[key_len(4) + key + offset(8) + type(1)]
  for (size_t i = 0; i < entries.size(); ++i) {
    const std::string &key = entries[i].key;
    auto key_len = static_cast<uint32_t>(key.size());
    uint64_t offset = offsets[i];
    auto type_byte = static_cast<uint8_t>(entries[i].type);

    file.write(reinterpret_cast<const char *>(&key_len), sizeof(key_len));
    file.write(key.data(), key.size());
    file.write(reinterpret_cast<const char *>(&offset), sizeof(offset));
    file.write(reinterpret_cast<const char *>(&type_byte), sizeof(type_byte));
  }

  // 5.写入 Bloom Filter
  // 5.1 收集所有 key
  std::vector<std::string> keys;
  keys.reserve(entries.size());
  for (const auto &entry : entries) {
    keys.emplace_back(entry.key);
  }

  // 5.2 构建 Bloom Filter
  bloom_filter_ = BloomFilter(entries.size());
  bloom_filter_.add_all(keys);

  // 5.3 序列化 Bloom Filter 并写入文件
  std::string bf_data = bloom_filter_.serialize();
  // 记录 Bloom Filter 区起始偏移 (用于 Footer 的 reserved 字段）
  uint64_t bloom_offset = static_cast<uint64_t>(file.tellp());

  auto bf_size = static_cast<uint32_t>(bf_data.size());
  file.write(reinterpret_cast<const char *>(&bf_size), sizeof(bf_size));
  file.write(bf_data.data(), bf_data.size());

  // 6.写入 Footer
  SSTableFooter footer{};
  std::memcpy(footer.magic, "SST\0", 4);
  footer.num_entries = static_cast<uint32_t>(entries.size());
  footer.data_end_offset = static_cast<uint64_t>(data_end);
  footer.version = 3;
  footer.checksum = checksum;
  footer.reserved = static_cast<uint32_t>(bloom_offset);
  file.write(reinterpret_cast<const char *>(&footer), sizeof(footer));

  return file.good();
}

std::optional<std::string> SSTable::get(const std::string &key) const {
  // 加载索引
  if (!load_index())
    return std::nullopt;

  // Bloom Filter 预检查
  if (!bloom_filter_.may_contain(key)) {
    return std::nullopt;
  }

  // 二分查找 key
  auto it = std::lower_bound(index_.begin(), index_.end(), key,
                             [](const IndexEntry &entry, const std::string &k) {
                               return entry.key < k;
                             });
  if (it == index_.end() || it->key != key)
    return std::nullopt;

  // 根据 offset 读取 Entry
  std::optional<Entry> entry_opt;

  if (version_ >= 3) {
    // v3：从解压缓冲区读取
    if (!load_data())
      return std::nullopt;

    const char *ptr = decompressed_data_.data() + it->offset;
    uint32_t len;
    std::memcpy(&len, ptr, sizeof(len));
    std::string data(ptr + sizeof(len), len);

    entry_opt = Entry::decode(data);
  } else {
    // v1/v2：从文件直接读取
    std::ifstream file(file_path_, std::ios::binary);
    if (!file)
      return std::nullopt;

    file.seekg(it->offset);
    uint32_t len;
    file.read(reinterpret_cast<char *>(&len), sizeof(len));
    std::string data(len, '\0');
    file.read(data.data(), len);

    entry_opt = Entry::decode(data);
  }

  if (!entry_opt.has_value())
    return std::nullopt;

  const Entry &entry = entry_opt.value();

  // 检查是否是墓碑
  if (entry.is_tombstone())
    return std::nullopt;

  return entry.value;
}

std::vector<std::string> SSTable::keys() const {
  std::vector<std::string> result;

  if (!load_index())
    return result;

  result.reserve(index_.size());
  for (const auto &entry : index_) {
    if (entry.type != OpType::DELETE) {
      result.emplace_back(entry.key);
    }
  }

  return result;
}

std::vector<std::string> SSTable::all_keys() const {
  std::vector<std::string> result;

  if (!load_index())
    return result;

  result.reserve(index_.size());
  for (const auto &entry : index_) {
    result.emplace_back(entry.key);
  }

  return result;
}

std::vector<Entry> SSTable::get_all_entries() const {
  if (!load_index())
    return {};

  std::vector<Entry> entries;
  entries.reserve(index_.size());

  if (version_ >= 3) {
    // v3：加载并解压数据区，从解压缓冲区读取
    if (!load_data())
      return {};

    for (const auto &idx : index_) {
      const char *ptr = decompressed_data_.data() + idx.offset;
      uint32_t len;
      std::memcpy(&len, ptr, sizeof(len));
      std::string data(ptr + sizeof(len), len);

      auto entry_opt = Entry::decode(data);
      if (entry_opt.has_value())
        entries.emplace_back(std::move(entry_opt.value()));
    }
  } else {
    // v1/v2：从文件直接读取
    std::ifstream file(file_path_, std::ios::binary);
    if (!file)
      return {};

    for (const auto &idx : index_) {
      file.seekg(idx.offset);
      uint32_t len;
      file.read(reinterpret_cast<char *>(&len), sizeof(len));
      std::string data(len, 0);
      file.read(data.data(), len);

      auto entry_opt = Entry::decode(data);
      if (entry_opt.has_value())
        entries.emplace_back(std::move(entry_opt.value()));
    }
  }

  return entries;
}

const std::vector<IndexEntry> &SSTable::index_entries() const {
  load_index();
  return index_;
}

bool SSTable::load_index() const {
  if (!index_.empty())
    return true;

  std::ifstream file(file_path_, std::ios::binary);
  if (!file) {
    std::cerr << "[Warning] SSTable::load_index: failed to open file: "
              << file_path_ << std::endl;
    return false;
  }

  // 读取 Footer
  file.seekg(-static_cast<int>(sizeof(SSTableFooter)), std::ios::end);
  SSTableFooter footer{};
  file.read(reinterpret_cast<char *>(&footer), sizeof(footer));

  // 验证 magic
  if (std::memcmp(footer.magic, "SST\0", 4) != 0) {
    std::cerr << "[Warning] SSTable::load_index: invalid magic in file: "
              << file_path_ << std::endl;
    return false;
  }

  // 跳到索引区起始位置
  file.seekg(footer.data_end_offset);

  // 从索引区起始位置开始，逐个读取索引项
  for (uint32_t i = 0; i < footer.num_entries; ++i) {
    // 读取：[key_len(4) + key + offset(8) + type(1)]
    uint32_t key_len;
    file.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));

    std::string key(key_len, '\0');
    file.read(key.data(), key_len);

    uint64_t offset;
    file.read(reinterpret_cast<char *>(&offset), sizeof(offset));

    uint8_t type_byte;
    file.read(reinterpret_cast<char *>(&type_byte), sizeof(type_byte));
    auto type = static_cast<OpType>(type_byte);

    index_.push_back({key, offset, type});
  }

  // 加载 Bloom Filter
  if (footer.version >= 2 && footer.reserved > 0) {
    file.seekg(footer.reserved);
    uint32_t bf_size;
    file.read(reinterpret_cast<char *>(&bf_size), sizeof(bf_size));
    std::string bf_data(bf_size, '\0');
    file.read(bf_data.data(), bf_size);
    bloom_filter_ = BloomFilter::deserialize(bf_data);
  }

  version_ = footer.version;
  return true;
}

bool SSTable::load_data() const {
  if (data_loaded_)
    return true;

  std::ifstream file(file_path_, std::ios::binary);
  if (!file)
    return false;

  // 读取 Footer 获取 data_end_offset
  file.seekg(-static_cast<int>(sizeof(SSTableFooter)), std::ios::end);
  SSTableFooter footer{};
  file.read(reinterpret_cast<char *>(&footer), sizeof(footer));
  if (std::memcmp(footer.magic, "SST\0", 4) != 0)
    return false;

  if (footer.version < 3) {
    // v1/v2 数据区未压缩，由 get() 直接从文件 seek 读取
    return false;
  }

  // v3: seek 到文件开头读取压缩数据区
  file.seekg(0, std::ios::beg);

  uint32_t uncompressed_size; // 原始数据大小
  file.read(reinterpret_cast<char *>(&uncompressed_size),
            sizeof(uncompressed_size));

  uint32_t compressed_size; // 压缩数据大小
  file.read(reinterpret_cast<char *>(&compressed_size),
            sizeof(compressed_size));

  std::string compressed_data(compressed_size, '\0'); // 压缩数据
  file.read(compressed_data.data(), compressed_size);

  // 校验 checksum
  uint32_t checksum = compute_checksum(compressed_data);
  if (footer.checksum != checksum) {
    std::cerr << "[Warning] SSTable::load_data: checksum mismatch in file: "
              << file_path_ << std::endl;
    return false;
  }

  // 解压
  decompressed_data_.resize(uncompressed_size);
  int result = LZ4_decompress_safe(
      compressed_data.data(), decompressed_data_.data(),
      static_cast<int>(compressed_size), static_cast<int>(uncompressed_size));
  if (result < 0)
    return false;

  data_loaded_ = true;
  return true;
}

} // namespace x1nglsm::core