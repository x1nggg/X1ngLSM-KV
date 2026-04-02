/**
 * @file example_basic.cpp
 * @brief X1ngLSM-KV 基础操作示例
 *
 * 展示最基本的 CRUD 操作：
 * - PUT: 写入 key-value
 * - GET: 查询 key
 * - DELETE: 删除 key
 * - EXISTS: 检查 key 是否存在
 */

#include "x1nglsm/kv_store.hpp"
#include <cassert>
#include <iostream>

int main() {
  std::cout << "=== X1ngLSM-KV 基础操作示例 ===\n\n";

  // 1. 创建 KVStore，指定数据目录
  std::cout << "1. 创建 KVStore...\n";
  x1nglsm::KVStore store("./data/example_basic");
  std::cout << "   ✓ KVStore 创建成功\n\n";

  // 2. PUT 操作 - 写入数据
  std::cout << "2. PUT 操作 - 写入数据:\n";
  store.put("user:1", "Alice");
  store.put("user:2", "Bob");
  store.put("user:3", "Charlie");
  std::cout << "   ✓ 写入: user:1 -> Alice\n";
  std::cout << "   ✓ 写入: user:2 -> Bob\n";
  std::cout << "   ✓ 写入: user:3 -> Charlie\n\n";

  // 3. GET 操作 - 读取数据
  std::cout << "3. GET 操作 - 读取数据:\n";
  auto result1 = store.get("user:1");
  if (result1.has_value()) {
    std::cout << "   ✓ GET user:1 = " << result1.value() << "\n";
  }

  auto result2 = store.get("user:999"); // 不存在的 key
  if (!result2.has_value()) {
    std::cout << "   ✓ GET user:999 = (不存在)\n";
  }
  std::cout << "\n";

  // 4. EXISTS 操作 - 检查 key 是否存在
  std::cout << "4. EXISTS 操作 - 检查 key 是否存在:\n";
  std::cout << "   ✓ EXISTS user:1 = "
            << (store.exists("user:1") ? "true" : "false") << "\n";
  std::cout << "   ✓ EXISTS user:999 = "
            << (store.exists("user:999") ? "true" : "false") << "\n\n";

  // 5. DELETE 操作 - 删除数据
  std::cout << "5. DELETE 操作 - 删除数据:\n";
  store.remove("user:2");
  std::cout << "   ✓ 删除: user:2\n";

  auto deleted_result = store.get("user:2");
  if (!deleted_result.has_value()) {
    std::cout << "   ✓ 验证: user:2 已被删除\n\n";
  }

  // 6. KEYS 操作 - 获取所有 key
  std::cout << "6. KEYS 操作 - 获取所有 key:\n";
  auto keys = store.keys();
  std::cout << "   当前共有 " << keys.size() << " 个 key:\n";
  for (size_t i = 0; i < keys.size(); ++i) {
    std::cout << "     " << (i + 1) << ". " << keys[i] << "\n";
  }
  std::cout << "\n";

  // 7. INFO - 统计信息
  std::cout << "7. 统计信息:\n";
  std::cout << "   内存使用: " << store.mem_usage() << " 字节\n";
  std::cout << "   Key 数量: " << store.size() << "\n";
  std::cout << "   SSTable 数量: " << store.sstables_count() << "\n\n";

  std::cout << "=== 示例完成 ===\n";
  return 0;
}
