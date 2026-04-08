#include "x1nglsm/core/bloom_filter.hpp"

#include <cmath>
#include <cstring>
#include <functional>

namespace x1nglsm::core {

BloomFilter::BloomFilter(size_t expected_items, double fp_rate) {
  // 根据预期元素数量和误判率，计算最优参数
  bit_count_ = optimal_bit_count(expected_items, fp_rate);
  num_hashes_ = optimal_num_hashes(bit_count_, expected_items);
  // 分配位数组，每个字节管 8 个 bit，向上取整
  bits_.resize((bit_count_ + 7) / 8, 0);
}

void BloomFilter::add(const std::string &key) {
  // 对 key 做 num_hashes_ 次哈希，将对应的 bit 全部置为 1
  for (size_t i = 0; i < num_hashes_; ++i) {
    uint64_t pos = hash(key, i);        // 第 i 次哈希的位位置
    bits_[pos / 8] |= (1 << (pos % 8)); // 对应字节中的对应 bit 置 1
  }
}

void BloomFilter::add_all(const std::vector<std::string> &keys) {
  for (const auto &key : keys) {
    add(key);
  }
}

bool BloomFilter::may_contain(const std::string &key) const {
  // 如果 Bloom Filter 未加载，返回true表示可能存在，后续进行二分查找进一步判断
  if (bit_count_ == 0) {
    return true;
  }

  // 检查 key 的所有哈希位是否都为 1
  // 有一个 bit 为 0 → 一定不存在，立即返回 false
  // 全部为 1 → 可能存在（有小概率误判）
  for (size_t i = 0; i < num_hashes_; ++i) {
    uint64_t pos = hash(key, i);
    if ((bits_[pos / 8] & (1 << (pos % 8))) == 0) {
      return false;
    }
  }

  return true;
}

std::string BloomFilter::serialize() const {
  // 格式：[num_hashes_(4字节)][bit_count_(8字节)][bits_原始数据]
  std::string result;

  // 写入哈希函数数量（4 字节）
  uint32_t nh = static_cast<uint32_t>(num_hashes_);
  result.append(reinterpret_cast<const char *>(&nh), sizeof(nh));

  // 写入位数组总 bit 数（8 字节）
  result.append(reinterpret_cast<const char *>(&bit_count_),
                sizeof(bit_count_));

  // 写入位数组原始字节
  result.append(reinterpret_cast<const char *>(bits_.data()), bits_.size());

  return result;
}

BloomFilter BloomFilter::deserialize(const std::string &data) {
  // 按 serialize 的格式依次读回
  BloomFilter bf(1); // 临时构造，后续会覆盖所有成员
  size_t offset = 0;

  // 读取哈希函数数量
  uint32_t nh;
  std::memcpy(&nh, data.data() + offset, sizeof(nh));
  offset += sizeof(nh);
  bf.num_hashes_ = nh;

  // 读取位数组总 bit 数
  std::memcpy(&bf.bit_count_, data.data() + offset, sizeof(bf.bit_count_));
  offset += sizeof(bf.bit_count_);

  // 读取位数组原始字节
  size_t byte_count = (bf.bit_count_ + 7) / 8;
  bf.bits_.resize(byte_count);
  std::memcpy(bf.bits_.data(), data.data() + offset, byte_count);

  return bf;
}

uint64_t BloomFilter::hash(const std::string &key, size_t hash_idx) const {
  // 双哈希法：只算两次哈希，通过线性组合模拟任意多次
  // 第 i 次哈希 = (h1 + i * h2) % bit_count_
  // 避免实现多个独立的哈希函数
  uint64_t h1 = std::hash<std::string>{}(key);

  // 第二次哈希：对 key 加一个字节，产生不同的哈希值
  std::string modified = key;
  modified += '\0';
  uint64_t h2 = std::hash<std::string>{}(modified);

  return (h1 + hash_idx * h2) % bit_count_;
}

size_t BloomFilter::optimal_num_hashes(size_t bits, size_t items) {
  // 公式：k = (m / n) * ln2
  // m = 位数组大小，n = 元素数量
  // 返回值至少为 1，防止除零或无效结果
  double k =
      (static_cast<double>(bits) / static_cast<double>(items) * std::log(2.0));
  return static_cast<size_t>(k) < 1 ? 1 : static_cast<size_t>(k);
}

size_t BloomFilter::optimal_bit_count(size_t items, double fp_rate) {
  // 公式：m = -(n * ln(p)) / (ln2)^2
  // n = 元素数量，p = 目标误判率
  // 返回值至少为 1
  double ln2 = std::log(2.0);
  double m = -(static_cast<double>(items) * std::log(fp_rate)) / (ln2 * ln2);
  return static_cast<size_t>(m) < 1 ? 1 : static_cast<size_t>(m);
}

} // namespace x1nglsm::core