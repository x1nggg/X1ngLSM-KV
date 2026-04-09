#include "x1nglsm/core/skip_list.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace x1nglsm::core;

// ========== 测试用例 ==========

// test_empty_find - 空表查询应返回 nullptr
bool test_empty_find() {
  SkipList<std::string, std::string> list;

  if (list.find("nonexistent") != nullptr)
    return false;
  if (!list.empty())
    return false;
  if (list.size() != 0)
    return false;

  return true;
}

// test_insert_and_find - 插入后能查到
bool test_insert_and_find() {
  SkipList<std::string, std::string> list;
  list.insert("hello", "world");

  auto *node = list.find("hello");
  if (node == nullptr)
    return false;
  if (node->key != "hello")
    return false;
  if (node->value != "world")
    return false;

  return true;
}

// test_update_value - 重复插入同一 key 应覆盖 value
bool test_update_value() {
  SkipList<std::string, std::string> list;
  list.insert("key", "old_value");
  list.insert("key", "new_value");

  if (list.size() != 1)
    return false;

  auto *node = list.find("key");
  if (node == nullptr)
    return false;
  if (node->value != "new_value")
    return false;

  return true;
}

// test_ordered_traversal - 遍历第 0 层应按 key 有序
bool test_ordered_traversal() {
  SkipList<std::string, int> list;

  // 乱序插入
  list.insert("delta", 4);
  list.insert("alpha", 1);
  list.insert("charlie", 3);
  list.insert("bravo", 2);
  list.insert("echo", 5);

  if (list.size() != 5)
    return false;

  // 遍历第 0 层，应该按字母序
  std::vector<std::string> keys;
  for (auto *node = list.header()->forward[0]; node != nullptr;
       node = node->forward[0]) {
    keys.push_back(node->key);
  }

  if (keys.size() != 5)
    return false;
  if (keys[0] != "alpha" || keys[1] != "bravo" || keys[2] != "charlie" ||
      keys[3] != "delta" || keys[4] != "echo")
    return false;

  return true;
}

// test_clear - 清空后应为空表
bool test_clear() {
  SkipList<std::string, std::string> list;
  list.insert("a", "1");
  list.insert("b", "2");
  list.insert("c", "3");

  list.clear();

  if (!list.empty())
    return false;
  if (list.size() != 0)
    return false;
  if (list.find("a") != nullptr)
    return false;
  if (list.find("b") != nullptr)
    return false;
  if (list.find("c") != nullptr)
    return false;

  return true;
}

// test_large_insert - 大量插入后 size 和 find 都正确
bool test_large_insert() {
  SkipList<int, int> list;

  const int N = 1000;
  for (int i = 0; i < N; ++i) {
    list.insert(i, i * 10);
  }

  if (list.size() != static_cast<size_t>(N))
    return false;

  // 验证每个 key 都能找到，且 value 正确
  for (int i = 0; i < N; ++i) {
    auto *node = list.find(i);
    if (node == nullptr)
      return false;
    if (node->value != i * 10)
      return false;
  }

  // 不存在的 key 应返回 nullptr
  if (list.find(N) != nullptr)
    return false;
  if (list.find(-1) != nullptr)
    return false;

  return true;
}

// test_large_ordered - 大量插入后遍历仍有序
bool test_large_ordered() {
  SkipList<int, int> list;

  const int N = 500;
  // 逆序插入
  for (int i = N - 1; i >= 0; --i) {
    list.insert(i, i);
  }

  // 遍历应该是升序
  int expected = 0;
  for (auto *node = list.header()->forward[0]; node != nullptr;
       node = node->forward[0]) {
    if (node->key != expected)
      return false;
    expected++;
  }

  if (expected != N)
    return false;

  return true;
}

// ========== 主函数 ==========

int main() {
  int passed = 0;
  int total = 0;

  struct TestCase {
    const char *name;
    bool (*func)();
  };

  TestCase tests[] = {
      {"test_empty_find", test_empty_find},
      {"test_insert_and_find", test_insert_and_find},
      {"test_update_value", test_update_value},
      {"test_ordered_traversal", test_ordered_traversal},
      {"test_clear", test_clear},
      {"test_large_insert", test_large_insert},
      {"test_large_ordered", test_large_ordered},
  };

  int num_tests = sizeof(tests) / sizeof(tests[0]);

  for (int i = 0; i < num_tests; i++) {
    total++;
    try {
      if (tests[i].func()) {
        std::cout << "[PASS] " << tests[i].name << std::endl;
        passed++;
      } else {
        std::cout << "[FAIL] " << tests[i].name << std::endl;
      }
    } catch (const std::exception &e) {
      std::cout << "[ERROR] " << tests[i].name << " - " << e.what()
                << std::endl;
    } catch (...) {
      std::cout << "[ERROR] " << tests[i].name << " (unknown exception)"
                << std::endl;
    }
  }

  std::cout << "\n=================================" << std::endl;
  std::cout << "Passed: " << passed << "/" << total << std::endl;

  if (passed == total) {
    std::cout << "All tests passed!" << std::endl;
    return 0;
  } else {
    std::cout << "Some tests failed!" << std::endl;
    return 1;
  }
}
