#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace x1nglsm::core {

/**
 * @brief 布隆过滤器（Bloom Filter）
 * @details
 * 概率型数据结构，用于快速判断 key "可能存在" 或 "一定不存在"。
 * 在 SSTable 查询前先检查 Bloom Filter，跳过不存在的 key，减少磁盘 IO。
 *
 * 原理：
 * - 内部维护一个位数组，初始全为 0
 * - 插入时对 key 做多次哈希，将对应位置为 1
 * - 查询时检查所有哈希位是否都为 1
 * - "全为 1" → 可能存在（有误判率）
 * - "有 0" → 一定不存在（不会漏判）
 */
class BloomFilter {
public:
  BloomFilter() : bit_count_(0), num_hashes_(0) {};
  /**
   * @brief 构造函数
   * @param expected_items 预期元素数量，用于计算位数组大小
   * @param fp_rate 目标误判率，默认 0.01（1%）
   */
  explicit BloomFilter(size_t expected_items, double fp_rate = 0.01);

  ~BloomFilter() = default;

  // 添加单个 key
  void add(const std::string &key);

  // 批量添加 key
  void add_all(const std::vector<std::string> &keys);

  /**
   * @brief 判断 key 是否可能存在
   * @return true 可能存在（有小概率误判）
   *         false 一定不存在
   */
  bool may_contain(const std::string &key) const;

  // 序列化为字节串，用于写入 SSTable 文件
  std::string serialize() const;

  // 从字节串反序列化，用于从 SSTable 文件加载
  static BloomFilter deserialize(const std::string &data);

  // 获取位数组大小（bit 数）
  size_t bit_count() const { return bit_count_; }

  // 获取哈希函数数量
  size_t hash_count() const { return num_hashes_; }

private:
  // 双哈希法：基于 h1 和 h2 生成第 hash_idx 个哈希值
  uint64_t hash(const std::string &key, size_t hash_idx) const;

  // 根据位数组大小和元素数量计算最优哈希函数数量
  static size_t optimal_num_hashes(size_t bits, size_t items);

  // 根据元素数量和误判率计算最优位数组大小
  static size_t optimal_bit_count(size_t items, double fp_rate);

  // 位数组，每个 uint8_t 管理 8 个 bit
  std::vector<uint8_t> bits_;
  // 位数组总 bit 数
  size_t bit_count_;
  // 哈希函数数量
  size_t num_hashes_;
};

} // namespace x1nglsm::core