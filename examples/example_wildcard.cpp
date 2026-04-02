/**
 * @file example_wildcard.cpp
 * @brief X1ngLSM-KV 通配符查询示例
 *
 * 展示通配符模式匹配功能：
 * - * 匹配 0 个或多个字符
 * - ? 匹配单个字符
 */

#include "x1nglsm/kv_store.hpp"
#include "x1nglsm/utils/glob_utils.hpp"
#include <iostream>
#include <string>
#include <vector>

int main() {
  std::cout << "=== X1ngLSM-KV 通配符查询示例 ===\n\n";

  // 1. 创建 KVStore
  std::cout << "1. 创建 KVStore 并准备测试数据...\n";
  x1nglsm::KVStore store("./data/example_wildcard");

  // 准备测试数据
  std::vector<std::pair<std::string, std::string>> test_data = {
      {"user:1", "Alice"},           {"user:2", "Bob"},
      {"user:100", "Charlie"},       {"session:a1", "data1"},
      {"session:b2", "data2"},       {"session:c99", "data3"},
      {"cache:item_1", "value1"},    {"cache:item_2", "value2"},
      {"cache:item_test", "value3"}, {"temp", "temporary"},
      {"log:2024:01:01", "entry1"},  {"log:2024:01:02", "entry2"}};

  for (const auto &kv : test_data) {
    store.put(kv.first, kv.second);
  }
  std::cout << "   ✓ 写入 " << test_data.size() << " 个测试 key\n\n";

  // 2. 测试通配符模式
  std::cout << "2. 测试通配符模式:\n\n";

  // 定义要测试的模式
  std::vector<std::string> patterns = {
      "user:*",    // 匹配所有 user: 开头的 key
      "user:?",    // 匹配 user: 后跟单个字符的 key
      "session:*", // 匹配所有 session: 开头的 key
      "session:?", // 匹配 session: 后跟单个字符的 key
      "cache:*",   // 匹配所有 cache: 开头的 key
      "*_test",    // 匹配以 _test 结尾的 key
      "*:2024:*",  // 匹配包含 :2024: 的 key
      "*"          // 匹配所有 key
  };

  for (const auto &pattern : patterns) {
    std::cout << "模式: \"" << pattern << "\"\n";

    // 获取所有 key
    auto all_keys = store.keys();

    // 使用通配符过滤
    std::vector<std::string> matched_keys;
    for (const auto &key : all_keys) {
      if (x1nglsm::utils::glob_match(pattern, key)) {
        matched_keys.push_back(key);
      }
    }

    // 显示匹配结果
    if (matched_keys.empty()) {
      std::cout << "   (无匹配)\n";
    } else {
      for (const auto &key : matched_keys) {
        auto value = store.get(key);
        std::cout << "   ✓ " << key << " = "
                  << (value.has_value() ? value.value() : "(null)") << "\n";
      }
    }
    std::cout << "   共 " << matched_keys.size() << " 个匹配\n\n";
  }

  // 3. 通配符使用技巧
  std::cout << "3. 通配符使用技巧:\n";
  std::cout << "   user:*        - 匹配所有用户数据\n";
  std::cout << "   session:*     - 匹配所有会话数据\n";
  std::cout << "   *_test        - 匹配所有测试相关的 key\n";
  std::cout << "   cache:item_?  - 匹配 cache:item_X 格式的 key\n";
  std::cout << "   *:2024:*      - 匹配特定年份的日志\n";
  std::cout << "   *             - 匹配所有 key\n\n";

  std::cout << "=== 示例完成 ===\n";
  return 0;
}
