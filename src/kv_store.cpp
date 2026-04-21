#include "x1nglsm/kv_store.hpp"
#include "x1nglsm/core/entry.hpp"
#include "x1nglsm/core/mem_table.hpp"
#include "x1nglsm/core/sstable.hpp"
#include "x1nglsm/core/write_ahead_log.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace x1nglsm {

KVStore::KVStore(std::string db_dir, size_t flush_threshold)
    : db_dir_(std::move(db_dir)), next_sst_id_(1),
      flush_threshold_(flush_threshold) {

  // 创建数据根目录
  std::filesystem::create_directories(db_dir_);

  // 创建分层目录
  levels_.resize(MAX_LEVEL);
  for (int i = 0; i < MAX_LEVEL; ++i) {
    std::filesystem::create_directories(level_dir(i));
  }

  mem_table_ = std::make_unique<core::MemTable>();
  wal_ = std::make_unique<core::WriteAheadLog>(db_dir_ + "/wal.log");

  // 恢复已有 SSTable
  recover_sstables();

  // 恢复数据
  recover_from_wal();
}

bool KVStore::put(const std::string &key, const std::string &value) {
  uint64_t timestamp = mem_table_->get_next_timestamp();
  core::Entry entry(key, value, core::OpType::PUT, timestamp);
  return write_to_wal_and_memtable(entry);
}

bool KVStore::put(const std::vector<std::pair<std::string, std::string>> &kvs) {
  for (const auto &kv : kvs) {
    if (!put(kv.first, kv.second))
      return false;
  }
  return true;
}

std::optional<std::string> KVStore::get(const std::string &key) const {
  // 先查 MemTable（最新数据）
  // 直接查 table_ 以区分墓碑和不存在，KVStore 是 MemTable 的友元
  auto mem_node = mem_table_->table_.find(key);
  if (!mem_node && immutable_mem_table_) {
    mem_node = immutable_mem_table_->table_.find(key);
  }

  if (mem_node) {
    // key 在 MemTable 中，如果是墓碑则立即返回 nullopt
    if (mem_node->value.is_tombstone())
      return std::nullopt;

    return mem_node->value.value;
  }

  // 再查 SSTable（Level 0 → Level 1 → ...，每层内从新到旧）
  for (int level = 0; level < MAX_LEVEL; ++level) {
    for (auto it = levels_[level].rbegin(); it != levels_[level].rend(); ++it) {
      const auto &entries = (*it)->index_entries();
      auto idx_it =
          std::lower_bound(entries.begin(), entries.end(), key,
                           [](const core::IndexEntry &entry,
                              const std::string &k) { return entry.key < k; });

      if (idx_it != entries.end() && idx_it->key == key) {
        if (idx_it->type == core::OpType::DELETE)
          return std::nullopt;

        auto sst_result = (*it)->get(key);
        if (sst_result.has_value())
          return sst_result;
      }
    }
  }

  return std::nullopt;
}

std::vector<std::optional<std::string>>
KVStore::get(const std::vector<std::string> &keys) const {
  std::vector<std::optional<std::string>> results;
  results.reserve(keys.size());
  for (const auto &key : keys) {
    results.emplace_back(get(key));
  }
  return results;
}

bool KVStore::remove(const std::string &key) {
  uint64_t timestamp = mem_table_->get_next_timestamp();
  core::Entry entry(key, "", core::OpType::DELETE, timestamp);
  return write_to_wal_and_memtable(entry);
}

void KVStore::recover_from_wal() {
  // 从 WAL 中读取所有 Entry
  std::vector<core::Entry> entries = wal_->read_all();

  // 恢复数据到 MemTable
  uint64_t max_ts = 0;
  for (const auto &entry : entries) {
    if (entry.timestamp > max_ts)
      max_ts = entry.timestamp;
    if (entry.type == core::OpType::PUT) {
      mem_table_->put(entry.key, entry.value, entry.timestamp);
    } else {
      mem_table_->remove(entry.key, entry.timestamp);
    }
  }

  // 恢复后推进时间戳，确保后续写入不会重复使用已存在的时间戳
  if (max_ts > 0) {
    mem_table_->advance_timestamp(max_ts + 1);
  }
}

void KVStore::recover_sstables() {
  for (int level = 0; level < MAX_LEVEL; ++level) {
    std::string dir = level_dir(level);
    if (!std::filesystem::exists(dir))
      continue;

    // 收集该层所有 .sst 文件
    std::vector<std::string> sst_files;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      if (entry.path().extension() == ".sst")
        sst_files.emplace_back(entry.path().string());
    }

    // 按文件名排序（确保按 ID 顺序加载）
    std::sort(sst_files.begin(), sst_files.end());

    // 加载到对应层
    for (const auto &file : sst_files) {
      auto sstable = std::make_unique<core ::SSTable>(file);
      levels_[level].emplace_back(std::move(sstable));
    }

    // 更新 next_sst_id_ (取所有层中最大文件 ID + 1)
    if (!sst_files.empty()) {
      std::string last_file = sst_files.back();
      size_t pos = last_file.find_last_of("/\\");
      std::string filename = last_file.substr(pos + 1);
      uint64_t last_id = std::stoull(filename.substr(0, filename.find('.')));

      if (last_id >= next_sst_id_)
        next_sst_id_ = last_id + 1;
    }
  }
}

void KVStore::maybe_flush() {
  if (mem_table_->total_encoded_size() < flush_threshold_)
    return;

  // 1. 如果有旧的 immutable 还没 flush，先 flush 它
  if (immutable_mem_table_) {
    flush_immutable();
  }

  // 2. 把当前 memtable move 给 immutable
  uint64_t next_ts =
      mem_table_->peek_next_timestamp(); // 继承旧的时间戳
  immutable_mem_table_ = std::move(mem_table_);

  // 3. 创建新的空 memtable
  mem_table_ = std::make_unique<core::MemTable>();
  mem_table_->advance_timestamp(next_ts);

  // 4. flush immutable 到 SSTable
  flush_immutable();
}

void KVStore::flush_immutable() {
  if (!immutable_mem_table_)
    return;

  // 1. 生成 SSTable 文件路径（写入 Level 0）
  std::string filename =
      level_dir(0) + "/" + std::to_string(next_sst_id_++) + ".sst";

  // 2. 从 Immutable MemTable 获取所有 Entry
  auto entries = immutable_mem_table_->get_all_entries();

  // 3. 写入 SSTable 文件
  auto sstable = std::make_unique<core::SSTable>(filename);
  if (!sstable->write_from_entries(entries))
    return;

  // 4. 添加到 Level 0
  levels_[0].emplace_back(std::move(sstable));

  // 5. 清空 Immutable MemTable 和 WAL
  immutable_mem_table_.reset();
  wal_->clear();

  // 6. 检查是否需要触发 compaction
  maybe_compact();
}

void KVStore::maybe_compact() {
  // 检查每层是否需要 compaction
  for (int level = 0; level < MAX_LEVEL; ++level) {
    if (levels_[level].size() >= COMPACTION_TRIGGER)
      compact(level);
  }
}

void KVStore::compact(int level) {
  std::vector<std::unique_ptr<core::SSTable>> &sstables = levels_[level];
  std::vector<core::Entry> all_entries;

  // 1. 选取该层所有 SSTable 收集 Entry
  for (const auto &sstable : sstables) {
    auto entries = sstable->get_all_entries();
    for (auto &entry : entries) {
      all_entries.emplace_back(std::move(entry));
    }
  }

  // 2. 排序：按 key 升序，相同 key 按时间戳降序（新的在前）
  std::sort(all_entries.begin(), all_entries.end(),
            [](const core::Entry &a, const core::Entry &b) {
              if (a.key != b.key) {
                return a.key < b.key;
              }

              return a.timestamp > b.timestamp;
            });

  // 3. 去重 + 墓碑处理
  bool is_bottom = true;
  for (int i = level + 2; i < MAX_LEVEL; ++i) {
    if (!levels_[i].empty()) {
      is_bottom = false;
      break;
    }
  }

  std::vector<core::Entry> merged;
  std::unordered_set<std::string> tombstone_keys;
  for (const auto &entry : all_entries) {
    if (!merged.empty() && merged.back().key == entry.key) {
      // 已经保留了最新版本，跳过旧版本
      continue;
    }
    if (entry.is_tombstone()) {
      if (is_bottom) {
        // 最底层丢弃墓碑
        tombstone_keys.insert(entry.key);
        continue;
      }

      merged.emplace_back(entry);
    } else if (tombstone_keys.count(entry.key)) {
      // 跳过被墓碑覆盖的旧版本
      continue;
    } else {
      merged.emplace_back(entry);
    }
  }

  // 4. 全部是墓碑且在最底层，直接删除旧文件
  if (merged.empty()) {
    for (const auto &sstable : sstables) {
      std::filesystem::remove(sstable->file_path());
    }

    levels_[level].clear();
    return;
  }

  // 5. 写入新的 SSTable 到 Level level + 1
  int next_level = level + 1;
  std::string filename =
      level_dir(next_level) + "/" + std::to_string(next_sst_id_++) + ".sst";
  auto new_sstable = std::make_unique<core::SSTable>(filename);

  if (!new_sstable->write_from_entries(merged))
    return;

  // 6. 删除旧文件
  for (const auto &sstable : sstables) {
    std::filesystem::remove(sstable->file_path());
  }

  // 7. 替换：清空当前层，新 SSTable 加入下一层
  levels_[level].clear();
  levels_[next_level].emplace_back(std::move(new_sstable));
}

bool KVStore::write_to_wal_and_memtable(const core::Entry &entry) {
  // 先写 WAL
  if (!wal_->append(entry))
    return false;

  // 再写 MemTable（使用 Entry 中的时间戳）
  if (entry.type == core::OpType::PUT) {
    mem_table_->put(entry.key, entry.value, entry.timestamp);
  } else {
    mem_table_->remove(entry.key, entry.timestamp);
  }

  // 检查是否需要 Flush
  if (mem_table_->total_encoded_size() >= flush_threshold_)
    maybe_flush();

  return true;
}

bool KVStore::remove(const std::vector<std::string> &keys) {
  for (const auto &key : keys) {
    if (!remove(key))
      return false;
  }
  return true;
}

bool KVStore::exists(const std::string &key) const {
  return get(key).has_value();
}

void KVStore::clear() {
  // 1. 清空 MemTable 和 Immutable MemTable
  mem_table_->clear();
  immutable_mem_table_.reset();

  // 2. 清空 WAL
  wal_->clear();

  // 3. 删除所有 SSTable 文件
  for (int level = 0; level < MAX_LEVEL; ++level) {
    for (auto it = levels_[level].begin(); it != levels_[level].end(); ++it) {
      std::filesystem::remove((*it)->file_path());
    }
  }

  // 4. 清空 SSTable 内存列表
  for (int level = 0; level < MAX_LEVEL; ++level) {
    levels_[level].clear();
  }

  // 5. 重置 SSTable ID
  next_sst_id_ = 1;
}

std::vector<std::string> KVStore::keys() const {
  std::vector<std::string> result;
  std::unordered_set<std::string> seen;

  // 1. 从 MemTable 获取所有 key（优先，最新数据）
  // 墓碑 key 也要加入 seen，阻止旧 SSTable 中的同名 key 出现
  auto mem_entries = mem_table_->get_all_entries();
  for (auto &entry : mem_entries) {
    seen.insert(entry.key);
    if (!entry.is_tombstone())
      result.emplace_back(std::move(entry.key));
  }

  // 从 Immutable MemTable 获取 key（优先级仅次于活跃 MemTable）
  if (immutable_mem_table_) {
    auto immutable_entries = immutable_mem_table_->get_all_entries();
    for (auto &entry : immutable_entries) {
      seen.insert(entry.key);
      if (!entry.is_tombstone())
        result.emplace_back(std::move(entry.key));
    }
  }

  // 2. 从 SSTable 获取 key（Level 0 → Level 1 → ...，每层内从新到旧）
  // 使用 index_entries() 获取类型信息，正确处理跨 SSTable 墓碑
  for (int level = 0; level < MAX_LEVEL; ++level) {
    for (auto it = levels_[level].rbegin(); it != levels_[level].rend(); ++it) {
      const auto &entries = (*it)->index_entries();

      for (const auto &entry : entries) {
        if (seen.find(entry.key) == seen.end()) {
          seen.insert(entry.key);
          if (entry.type != core::OpType::DELETE)
            result.emplace_back(entry.key);
        }
      }
    }
  }

  return result;
}

size_t KVStore::sstables_count() const {
  size_t count = 0;
  for (int level = 0; level < MAX_LEVEL; ++level) {
    count += levels_[level].size();
  }

  return count;
}

size_t KVStore::mem_usage() const {
  size_t total = mem_table_->total_encoded_size();
  if (immutable_mem_table_)
    total += immutable_mem_table_->total_encoded_size();

  return total;
}

} // namespace x1nglsm