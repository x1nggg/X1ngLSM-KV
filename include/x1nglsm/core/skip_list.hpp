#pragma once

#include <cstddef>
#include <random>
#include <utility>
#include <vector>

namespace x1nglsm::core {

/**
 * @brief 跳表节点
 * @details forward[i] 指向第 i 层中该节点的下一个节点
 */
template <typename Key, typename Value> struct SkipNode {
  Key key;
  Value value;
  std::vector<SkipNode *> forward;

  SkipNode() = default;

  /**
   * @brief 构造节点
   * @param k 键
   * @param v 值
   * @param level forward 数组大小，全部初始化为 nullptr
   */
  SkipNode(Key k, Value v, size_t level)
      : key(std::move(k)), value(std::move(v)), forward(level, nullptr) {}

  ~SkipNode() = default;
};

/**
 * @brief 跳表，概率平衡的有序数据结构
 * @details 期望 O(log n) 查找/插入。MAX_LEVEL = 16，每层 50% 晋升概率
 */
template <typename Key, typename Value> class SkipList {

  static constexpr size_t MAX_LEVEL = 16; // 最大层数

public:
  // 构造函数，创建哨兵头节点，forward 数组大小为 MAX_LEVEL
  SkipList() {
    header_ = new SkipNode<Key, Value>(Key{}, Value{}, MAX_LEVEL);
    num_current_level_ = 1;
    size_ = 0;
  }

  // 禁止拷贝，避免浅拷贝导致 double free
  SkipList(const SkipList &) = delete;

  SkipList &operator=(const SkipList &) = delete;

  // 析构函数，清空所有节点后释放头节点
  ~SkipList() {
    clear();
    delete header_;
  }

  /**
   * @brief 插入或更新
   * @return true 新插入，false key 已存在则更新 value
   */
  bool insert(const Key &key, const Value &value);

  /**
   * @brief 查找
   * @return 目标节点的 const 指针，未找到返回 nullptr
   */
  const SkipNode<Key, Value> *find(const Key &key) const;

  // 清空所有节点（不释放头节点）
  void clear();

  // 跳表是否为空
  bool empty() const { return size_ == 0; };

  // 获取哨兵头节点，用于外部遍历第 0 层链表
  SkipNode<Key, Value> *header() const { return header_; }

  // 获取节点数量
  size_t size() const { return size_; }

private:
  // 随机生成新节点层数，每层 50% 概率晋升
  size_t random_level() const;

  // 哨兵头节点，forward[i] 指向第 i 层的第一个真实节点
  SkipNode<Key, Value> *header_;
  // 当前跳表的有效层数
  size_t num_current_level_;
  // 节点数量
  size_t size_;
};

template <typename Key, typename Value>
bool SkipList<Key, Value>::insert(const Key &key, const Value &value) {
  // update[i]：记录第 i 层中最后一个 < 目标 key
  // 的结点，便于后续每一层插入新结点
  std::vector<SkipNode<Key, Value> *> update(MAX_LEVEL, nullptr);
  SkipNode<Key, Value> *current = header_;

  // 从最高层开始找位置
  for (int i = static_cast<int>(num_current_level_) - 1; i >= 0; --i) {
    while (current->forward[i] && current->forward[i]->key < key) {
      current = current->forward[i];
    }
    update[i] = current;
  }

  // current 现在是第 0 层中最后一个 < 目标 key 的结点
  // 检查是否已存在
  current = current->forward[0];
  if (current && current->key == key) { // 已存在，更新值后返回 false
    current->value = value;
    return false;
  }

  // 新结点
  size_t new_level = random_level();
  if (new_level > num_current_level_) {
    // 多出来的层，update 指向 header
    for (size_t i = num_current_level_; i < new_level; ++i) {
      update[i] = header_;
    }

    num_current_level_ = new_level;
  }

  // 创建新结点并插入
  auto *new_node = new SkipNode<Key, Value>(key, value, new_level);
  for (size_t i = 0; i < new_level; ++i) {
    new_node->forward[i] = update[i]->forward[i];
    update[i]->forward[i] = new_node;
  }

  size_++;
  return true;
}

template <typename Key, typename Value>
const SkipNode<Key, Value> *SkipList<Key, Value>::find(const Key &key) const {
  const SkipNode<Key, Value> *current = header_;

  // 从最高层开始找
  for (int i = static_cast<int>(num_current_level_) - 1; i >= 0; --i) {
    while (current->forward[i] && current->forward[i]->key < key) {
      current = current->forward[i];
    }
  }

  // current 现在是第 0 层中最后一个 < 目标 key 的结点
  current = current->forward[0];
  if (current && current->key == key) {
    return current;
  }

  return nullptr;
}

template <typename Key, typename Value> void SkipList<Key, Value>::clear() {
  // 只需遍历第一层，因为该层包含所有结点
  SkipNode<Key, Value> *current = header_->forward[0];
  while (current) {
    SkipNode<Key, Value> *next = current->forward[0];
    delete current;
    current = next;
  }

  for (size_t i = 0; i < MAX_LEVEL; ++i) {
    header_->forward[i] = nullptr; // 重置 header 的所有 forward 指针
  }

  num_current_level_ = 1;
  size_ = 0;
}

template <typename Key, typename Value>
size_t SkipList<Key, Value>::random_level() const {
  // 梅森旋转伪随机数生成器，用硬件熵源初始化种子（只初始化一次）
  static std::mt19937 gen(std::random_device{}());
  // 均匀整数分布，产生 0 或 1，概率各 50%
  static std::uniform_int_distribution<int> dist(0, 1);

  size_t level = 1;
  // 不断抛硬币：0（正面）则晋升一层，1（反面）则停止，最多到 MAX_LEVEL
  while (dist(gen) == 0 && level < MAX_LEVEL) {
    level++;
  }

  return level;
}

} // namespace x1nglsm::core