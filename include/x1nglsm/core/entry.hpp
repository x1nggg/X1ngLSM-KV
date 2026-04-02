#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace x1nglsm::core {

// 操作类型枚举
enum class OpType : uint8_t {
  PUT = 0,   // 写入/更新
  DELETE = 1 // 删除（墓碑）
};

/**
 * KV操作记录类型
 * Entry是整个系统核心数据单元，WAL、MemTable、SSTable 等操作的基本单位
 * 兼顾内存效率、序列化效率、易于解析
 */
struct Entry {
  Entry() = default;

  Entry(std::string key, std::string value, OpType type, uint64_t timestamp);

  ~Entry() = default;

  std::string key;    // 键
  std::string value;  // 值（DELETE时为空）
  OpType type;        // 操作类型
  uint64_t timestamp; // 时间戳（全局递增）

  // 判断是否是墓碑
  bool is_tombstone() const { return type == OpType::DELETE; }

  // 序列化：转换为字节流
  // 格式：[type(1)][timestamp(8)][klen(4)][key][vlen(4)][value]
  std::string encode() const;

  // 反序列化：从字节流解析
  static std::optional<Entry> decode(const std::string &data);

  // 获取序列化后的字节长度
  size_t encode_size() const;
};

} // namespace x1nglsm::core