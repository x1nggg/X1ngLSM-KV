#include "x1nglsm/core/sstable.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

namespace x1nglsm::core {

bool SSTable::write_from_entries(const std::vector<Entry> &entries) {
  std::ofstream file(file_path_, std::ios::binary);
  if (!file)
    return false;

  std::vector<uint64_t> offsets; // 记录每个 Entry 的偏移量

  // 1.写入所有 Entry（格式：长度 + 数据）
  for (const auto &entry : entries) {
    uint64_t offset = file.tellp();
    offsets.emplace_back(offset);

    std::string data = entry.encode();
    auto len = static_cast<uint32_t>(data.size());

    file.write(reinterpret_cast<const char *>(&len), sizeof(len));
    file.write(data.data(), data.size());
  }

  // 记录数据区结束位置（即索引区开始位置）
  std::streampos data_end = file.tellp();

  // 2.写入索引区
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

  // 3.写入 Footer
  SSTableFooter footer{};
  std::memcpy(footer.magic, "SST\0", 4);
  footer.num_entries = static_cast<uint32_t>(entries.size());
  footer.data_end_offset = static_cast<uint64_t>(data_end);
  footer.version = 1;
  footer.checksum = 0; // MVP阶段暂不实现，直接设为0
  footer.reserved = 0;

  file.write(reinterpret_cast<const char *>(&footer), sizeof(footer));

  return file.good();
}

std::optional<std::string> SSTable::get(const std::string &key) const {
  // 加载索引
  if (!load_index())
    return std::nullopt;

  // 二分查找 key
  auto it = std::lower_bound(index_.begin(), index_.end(), key,
                             [](const IndexEntry &entry, const std::string &k) {
                               return entry.key < k;
                             });
  if (it == index_.end() || it->key != key)
    return std::nullopt;

  // 根据 offset 读取 Entry
  std::ifstream file(file_path_, std::ios::binary);
  if (!file)
    return std::nullopt;

  // 跳到 Entry 位置
  file.seekg(it->offset);

  // 读取长度
  uint32_t len;
  file.read(reinterpret_cast<char *>(&len), sizeof(len));

  // 读取 Entry 数据
  std::string data(len, '\0');
  file.read(data.data(), len);

  // 解码 Entry
  auto entry_opt = Entry::decode(data);
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

  return true;
}

uint32_t SSTable::compute_checksum(const std::string &data) const {
  // TODO MVP 暂不实现校验和
  return 0;
}

} // namespace x1nglsm::core