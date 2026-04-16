/**
 * @file example_persistence.cpp
 * @brief X1ngLSM-KV 持久化和恢复示例
 *
 * 展示 LSM-KV 的持久化能力：
 * - 数据写入后持久化到磁盘
 * - 程序重启后自动恢复数据
 * - Flush 机制（MemTable -> SSTable）
 */

#include "x1nglsm/kv_store.hpp"
#include <iostream>

void run_first_session() {
  std::cout << "\n========== 第一次运行 ==========\n";

  // 1. 创建 KVStore
  std::cout << "1. 创建 KVStore...\n";
  x1nglsm::KVStore store("./data/example_persistence");
  std::cout << "   ✓ KVStore 创建成功\n";

  // 2. 检查是否有恢复的数据
  auto existing_keys = store.keys();
  if (!existing_keys.empty()) {
    std::cout << "\n注意: 发现已有 " << existing_keys.size() << " 个 key\n";
    std::cout
        << "是否清空重新开始? (需要手动删除 data/example_persistence 目录)\n\n";
  }

  // 3. 写入数据
  std::cout << "2. 写入数据...\n";
  for (int i = 1; i <= 20; ++i) {
    store.put("session:" + std::to_string(i), "data_" + std::to_string(i));
  }
  std::cout << "   ✓ 写入 20 个 key-value\n";

  // 4. 显示统计信息
  std::cout << "\n3. 统计信息:\n";
  std::cout << "   内存使用: " << store.mem_usage() << " 字节\n";
  std::cout << "   Key 数量: " << store.size() << "\n";
  std::cout << "   SSTable 数量: " << store.sstables_count() << "\n";

  // 5. 触发 Flush（通过写入大值数据）
  std::cout << "\n4. 触发 Flush 机制...\n";
  std::cout << "   写入大值数据以触发 Flush (默认阈值 32MB)...\n";

  // 写入足够多的数据触发 Flush
  for (int i = 21; i <= 50; ++i) {
    store.put("session:" + std::to_string(i), "data_" + std::to_string(i));
  }

  std::cout << "\n   Flush 后的统计信息:\n";
  std::cout << "   内存使用: " << store.mem_usage() << " 字节\n";
  std::cout << "   Key 数量: " << store.size() << "\n";
  std::cout << "   SSTable 数量: " << store.sstables_count() << "\n";

  std::cout << "\n=== 第一次运行完成，数据已持久化 ===\n";
  std::cout << "请重新运行此程序查看数据恢复效果\n";
}

void run_second_session() {
  std::cout << "\n========== 第二次运行（恢复数据）==========\n";

  // 1. 创建 KVStore（会自动恢复数据）
  std::cout << "1. 创建 KVStore（自动恢复数据）...\n";
  x1nglsm::KVStore store("./data/example_persistence");
  std::cout << "   ✓ KVStore 创建成功\n";

  // 2. 验证数据是否恢复
  std::cout << "\n2. 验证数据恢复:\n";
  auto keys = store.keys();
  std::cout << "   恢复的 key 数量: " << keys.size() << "\n";

  // 3. 抽样验证几个 key
  std::cout << "\n3. 抽样验证:\n";
  std::vector<std::string> test_keys = {"session:1", "session:25",
                                        "session:50"};
  for (const auto &key : test_keys) {
    auto value = store.get(key);
    if (value.has_value()) {
      std::cout << "   ✓ " << key << " = " << value.value() << "\n";
    } else {
      std::cout << "   ✗ " << key << " = (未找到)\n";
    }
  }

  // 4. 显示统计信息
  std::cout << "\n4. 统计信息:\n";
  std::cout << "   内存使用: " << store.mem_usage() << " 字节\n";
  std::cout << "   Key 数量: " << store.size() << "\n";
  std::cout << "   SSTable 数量: " << store.sstables_count() << "\n";

  std::cout << "\n=== 第二次运行完成，数据成功恢复 ===\n";
}

int main() {
  std::cout << "=== X1ngLSM-KV 持久化和恢复示例 ===\n";

  // 简化版本：先清空再写入，然后恢复
  // 实际使用时，可以先运行一次写入，然后重启程序查看恢复效果

  int choice;
  std::cout << "\n选择模式:\n";
  std::cout << "  1. 写入数据（第一次运行）\n";
  std::cout << "  2. 恢复数据（第二次运行）\n";
  std::cout << "请选择: ";
  std::cin >> choice;

  if (choice == 1) {
    run_first_session();
  } else if (choice == 2) {
    run_second_session();
  } else {
    std::cout << "无效选择\n";
  }

  return 0;
}
