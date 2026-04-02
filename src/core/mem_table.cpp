#include "x1nglsm/core/mem_table.hpp"

namespace x1nglsm::core {

MemTable::MemTable() : next_timestamp_(1), total_encoded_size_(0) {}

void MemTable::put(const std::string &key, const std::string &value,
                   std::optional<uint64_t> timestamp) {
  // 如果没有提供时间戳，自动生成
  uint64_t ts =
      timestamp.has_value() ? timestamp.value() : get_next_timestamp();

  // 创建Entry，设置PUT类型和timestamp
  Entry entry(key, value, OpType::PUT, ts);

  // 如果key已存在，先减去旧Entry的序列化大小
  auto it = table_.find(key);
  if (it != table_.end()) {
    total_encoded_size_ -= it->second.encode_size();
  }

  // 插入新的Entry，更新total_encoded_size_
  table_[key] = entry;
  total_encoded_size_ += entry.encode_size();
}

void MemTable::remove(const std::string &key,
                      std::optional<uint64_t> timestamp) {
  // 如果没有提供时间戳，自动生成
  uint64_t ts =
      timestamp.has_value() ? timestamp.value() : get_next_timestamp();

  // 创建墓碑Entry
  Entry entry(key, "", OpType::DELETE, ts);

  // 处理total_encoded_size_
  auto it = table_.find(key);
  if (it != table_.end()) {
    total_encoded_size_ -= it->second.encode_size();
  }

  // 插入到table_中
  table_[key] = entry;
  total_encoded_size_ += entry.encode_size();
}

std::optional<std::string> MemTable::get(const std::string &key) const {
  auto it = table_.find(key);

  // 判断key是否存在。若存在，判断是否是墓碑
  if (it == table_.end() || it->second.is_tombstone())
    return std::nullopt;

  return it->second.value;
}

std::vector<Entry> MemTable::get_all_entries() const {
  if (table_.empty())
    return {};

  std::vector<Entry> result;
  result.reserve(table_.size());

  for (const auto &it : table_) {
    result.emplace_back(it.second);
  }

  return result;
}

std::vector<std::string> MemTable::keys() const {
  std::vector<std::string> result;
  result.reserve(table_.size());

  for (const auto &it : table_) {
    // 只返回未被删除的key（非墓碑key）
    if (!it.second.is_tombstone()) {
      result.emplace_back(it.first);
    }
  }

  return result;
}

size_t MemTable::size() const {
  size_t count = 0;
  for (const auto &it : table_) {
    // 只计算未被删除的key（非墓碑key）
    if (!it.second.is_tombstone()) {
      count++;
    }
  }
  return count;
}

} // namespace x1nglsm::core