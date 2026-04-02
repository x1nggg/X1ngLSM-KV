#include "x1nglsm/kv_store.hpp"
#include "x1nglsm/core/entry.hpp"

#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <utility>

static constexpr size_t THRESHOLD = 32 * 1024 * 1024; // Flush 阈值（32MB）

namespace x1nglsm {

KVStore::KVStore(std::string db_dir)
    : db_dir_(std::move(db_dir)), next_sst_id_(1) {
  // 创建数据目录（如果不存在）
  std::filesystem::create_directories(db_dir_);

  // 创建 SSTable 目录
  sstable_dir_ = db_dir_ + "/sstables";
  std::filesystem::create_directories(sstable_dir_);

  // 创建 WAL
  wal_ = std::make_unique<core::WriteAheadLog>(db_dir_ + "/wal.log");

  // 恢复已有 SSTable
  recover_sstables();

  // 恢复数据
  recover_from_wal();
}

bool KVStore::put(const std::string &key, const std::string &value) {
  uint64_t timestamp = mem_table_.get_next_timestamp();
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
  auto mem_it = mem_table_.table_.find(key);
  if (mem_it != mem_table_.table_.end()) {
    // key 在 MemTable 中，如果是墓碑则立即返回 nullopt
    if (mem_it->second.is_tombstone())
      return std::nullopt;
    return mem_it->second.value;
  }

  // 再查 SSTable 列表
  // 注意：sstables_ 是按时间顺序排列的，新的在后面
  // 需要从新到旧顺序查找
  for (auto it = sstables_.rbegin(); it != sstables_.rend(); ++it) {
    // 先检查 index_entries 判断类型，避免墓碑被当作"未找到"继续搜索旧表
    const auto &entries = (*it)->index_entries();
    auto idx_it = std::lower_bound(
        entries.begin(), entries.end(), key,
        [](const core::IndexEntry &entry, const std::string &k) {
          return entry.key < k;
        });
    if (idx_it != entries.end() && idx_it->key == key) {
      // 找到了 key，如果是 DELETE 墓碑则立即返回 nullopt
      if (idx_it->type == core::OpType::DELETE)
        return std::nullopt;
      // 是 PUT 类型，调用 get 获取值
      auto sst_result = (*it)->get(key);
      if (sst_result.has_value())
        return sst_result;
    }
    // key 不在此 SSTable 中，继续搜索更旧的表
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
  uint64_t timestamp = mem_table_.get_next_timestamp();
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
      mem_table_.put(entry.key, entry.value, entry.timestamp);
    } else {
      mem_table_.remove(entry.key, entry.timestamp);
    }
  }

  // 恢复后推进时间戳，确保后续写入不会重复使用已存在的时间戳
  if (max_ts > 0) {
    mem_table_.advance_timestamp(max_ts + 1);
  }
}

void KVStore::recover_sstables() {
  // 检查目录是否存在
  if (!std::filesystem::exists(sstable_dir_))
    return;

  // 遍历 SSTable 目录，收集所有 .sst 文件
  std::vector<std::string> sst_files;
  for (const auto &entry : std::filesystem::directory_iterator(sstable_dir_)) {
    if (entry.path().extension() == ".sst") {
      sst_files.emplace_back(entry.path().string());
    }
  }

  // 按文件名排序（确保按 ID 顺序加载，从旧到新）
  std::sort(sst_files.begin(), sst_files.end());

  // 加载每个 SSTable 到内存列表
  for (const auto &file : sst_files) {
    auto sstable = std::make_unique<core::SSTable>(file);
    sstables_.emplace_back(std::move(sstable));
  }

  // 更新 next_sst_id_（取最大文件 ID + 1）
  if (!sst_files.empty()) {
    // 从最后一个文件名提取 ID
    std::string last_file = sst_files.back();
    size_t pos = last_file.find_last_of('/');
    if (pos == std::string::npos) {
      pos = last_file.find_last_of('\\');
    }
    std::string filename = last_file.substr(pos + 1);
    // filename 格式: "123.sst"
    uint64_t last_id = std::stoull(filename.substr(0, filename.find('.')));
    next_sst_id_ = last_id + 1;
  }
}

void KVStore::maybe_flush() {
  if (mem_table_.total_encoded_size() < THRESHOLD)
    return;

  // 1. 生成新的 SSTable 文件路径
  std::string filename =
      sstable_dir_ + "/" + std::to_string(next_sst_id_++) + ".sst";

  // 2. 从 MemTable 获取所有 Entry
  auto entries = mem_table_.get_all_entries();

  // 3. 写入 SSTable 文件
  auto sstable = std::make_unique<core::SSTable>(filename);
  if (!sstable->write_from_entries(entries))
    return;

  // 4. 添加到 SSTable 列表
  sstables_.emplace_back(std::move(sstable));

  // 5. 清空 MemTable 和 WAL
  mem_table_.clear();
  wal_->clear();
}

bool KVStore::write_to_wal_and_memtable(const core::Entry &entry) {
  // 先写 WAL
  if (!wal_->append(entry))
    return false;

  // 再写 MemTable（使用 Entry 中的时间戳）
  if (entry.type == core::OpType::PUT) {
    mem_table_.put(entry.key, entry.value, entry.timestamp);
  } else {
    mem_table_.remove(entry.key, entry.timestamp);
  }

  // 检查是否需要 Flush
  if (mem_table_.total_encoded_size() >= THRESHOLD)
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
  // 1. 清空 MemTable
  mem_table_.clear();

  // 2. 清空 WAL
  wal_->clear();

  // 3. 删除所有 SSTable 文件
  for (const auto &sst : sstables_) {
    std::string file_path = sst->file_path();
    std::filesystem::remove(file_path);
  }

  // 4. 清空 SSTable 内存列表
  sstables_.clear();

  // 5. 重置 SSTable ID
  next_sst_id_ = 1;
}

std::vector<std::string> KVStore::keys() const {
  std::vector<std::string> result;
  std::unordered_set<std::string> seen;

  // 1. 从 MemTable 获取所有 key（优先，最新数据）
  // 墓碑 key 也要加入 seen，阻止旧 SSTable 中的同名 key 出现
  auto mem_entries = mem_table_.get_all_entries();
  for (const auto &entry : mem_entries) {
    seen.insert(entry.key);
    if (!entry.is_tombstone()) {
      result.emplace_back(entry.key);
    }
  }

  // 2. 从 SSTable 获取 key（从新到旧，跳过已存在的）
  // 使用 index_entries() 获取类型信息，正确处理跨 SSTable 墓碑
  for (auto it = sstables_.rbegin(); it != sstables_.rend(); ++it) {
    const auto &entries = (*it)->index_entries();
    for (const auto &entry : entries) {
      if (seen.find(entry.key) == seen.end()) {
        seen.insert(entry.key);
        if (entry.type != core::OpType::DELETE) {
          result.emplace_back(entry.key);
        }
      }
    }
  }

  return result;
}

size_t KVStore::size() const { return keys().size(); }

} // namespace x1nglsm