/**
 * @file example_batch.cpp
 * @brief X1ngLSM-KV 批量操作示例
 *
 * 展示批量操作 API：
 * - 批量 PUT: 一次写入多个 key-value
 * - 批量 GET: 一次查询多个 key
 * - 批量 DELETE: 一次删除多个 key
 */

#include "x1nglsm/kv_store.hpp"

#include <iostream>
#include <utility>
#include <vector>

int main() {
  std::cout << "=== X1ngLSM-KV 批量操作示例 ===\n\n";

  // 1. 创建 KVStore
  std::cout << "1. 创建 KVStore...\n";
  x1nglsm::KVStore store("./data/example_batch");
  std::cout << "   ✓ KVStore 创建成功\n\n";

  // 2. 批量 PUT - 一次写入多个 key-value
  std::cout << "2. 批量 PUT - 写入 1000 个 key-value:\n";
  std::vector<std::pair<std::string, std::string>> kvs;
  for (int i = 1; i <= 1000; ++i) {
    kvs.emplace_back("key:" + std::to_string(i), "value:" + std::to_string(i));
  }

  bool success = store.put(kvs);
  std::cout << "   ✓ 批量写入 " << kvs.size() << " 个 key-value, "
            << (success ? "成功" : "失败") << "\n\n";

  // 3. 批量 GET - 一次查询多个 key
  std::cout << "3. 批量 GET - 查询多个 key:\n";

  // 准备要查询的 key
  std::vector<std::string> keys_to_get;
  keys_to_get.push_back("key:1");
  keys_to_get.push_back("key:100");
  keys_to_get.push_back("key:500");
  keys_to_get.push_back("key:999");
  keys_to_get.push_back("key:invalid"); // 不存在的 key

  auto results = store.get(keys_to_get);

  std::cout << "   查询结果:\n";
  for (size_t i = 0; i < keys_to_get.size(); ++i) {
    const auto &key = keys_to_get[i];
    const auto &value = results[i];
    if (value.has_value()) {
      std::cout << "     ✓ " << key << " = " << value.value() << "\n";
    } else {
      std::cout << "     ✗ " << key << " = (不存在)\n";
    }
  }
  std::cout << "\n";

  // 4. 批量 DELETE - 一次删除多个 key
  std::cout << "4. 批量 DELETE - 删除多个 key:\n";

  std::vector<std::string> keys_to_delete;
  keys_to_delete.push_back("key:1");
  keys_to_delete.push_back("key:2");
  keys_to_delete.push_back("key:3");

  size_t count_before = store.keys().size();
  bool delete_success = store.remove(keys_to_delete);
  size_t count_after = store.keys().size();

  std::cout << "   ✓ 删除前 key 数量: " << count_before << "\n";
  std::cout << "   ✓ 删除 " << keys_to_delete.size()
            << " 个 key: " << (delete_success ? "成功" : "失败") << "\n";
  std::cout << "   ✓ 删除后 key 数量: " << count_after << "\n\n";

  // 5. 性能对比
  std::cout << "5. 批量 vs 单次操作对比:\n";
  std::cout << "   批量操作适合场景:\n";
  std::cout << "     • 需要一次性处理多个相关 key\n";
  std::cout << "     • 减少函数调用开销\n";
  std::cout << "     • 提高代码简洁性\n\n";

  std::cout << "=== 示例完成 ===\n";
  return 0;
}
