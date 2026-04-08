#include "x1nglsm/core/bloom_filter.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace x1nglsm::core;

// ========== 测试用例 ==========

// test_empty_filter - 默认构造的 Bloom Filter 对任何 key 返回 true（不确定）
bool test_empty_filter() {
  BloomFilter bf;
  if (!bf.may_contain("any_key"))
    return false;
  if (bf.bit_count() != 0)
    return false;
  if (bf.hash_count() != 0)
    return false;
  return true;
}

// test_add_and_query - 添加后查询，已添加的 key 应返回 true
bool test_add_and_query() {
  BloomFilter bf(100);
  bf.add("hello");
  bf.add("world");

  if (!bf.may_contain("hello"))
    return false;
  if (!bf.may_contain("world"))
    return false;

  return true;
}

// test_nonexistent_key - 未添加的 key 大部分应返回 false
bool test_nonexistent_key() {
  BloomFilter bf(100);
  bf.add("hello");
  bf.add("world");

  // 这些 key 没添加过，大概率返回 false
  int false_count = 0;
  std::vector<std::string> absent = {"aaa", "bbb", "ccc", "ddd", "eee",
                                     "fff", "ggg", "hhh", "iii", "jjj"};
  for (const auto &key : absent) {
    if (!bf.may_contain(key))
      false_count++;
  }

  // 至少 8/10 应该返回 false
  return false_count >= 8;
}

// test_add_all - 批量添加后所有 key 都应返回 true
bool test_add_all() {
  BloomFilter bf(100);
  std::vector<std::string> keys = {"key1", "key2", "key3", "key4", "key5"};
  bf.add_all(keys);

  for (const auto &key : keys) {
    if (!bf.may_contain(key))
      return false;
  }

  return true;
}

// test_serialize_deserialize - 序列化后反序列化，功能不变
bool test_serialize_deserialize() {
  BloomFilter bf(100);
  bf.add("apple");
  bf.add("banana");
  bf.add("cherry");

  // 序列化
  std::string data = bf.serialize();

  // 反序列化
  BloomFilter bf2 = BloomFilter::deserialize(data);

  // 验证参数一致
  if (bf.bit_count() != bf2.bit_count())
    return false;
  if (bf.hash_count() != bf2.hash_count())
    return false;

  // 验证查询结果一致
  if (!bf2.may_contain("apple"))
    return false;
  if (!bf2.may_contain("banana"))
    return false;
  if (!bf2.may_contain("cherry"))
    return false;

  return true;
}

// test_false_positive_rate - 统计误判率，应在 2% 以下
bool test_false_positive_rate() {
  BloomFilter bf(10000, 0.01);

  // 插入 10000 个 key
  for (int i = 0; i < 10000; ++i) {
    bf.add("key_" + std::to_string(i));
  }

  // 查询另外 10000 个不存在的 key
  int false_positive = 0;
  for (int i = 10000; i < 20000; ++i) {
    if (bf.may_contain("key_" + std::to_string(i)))
      false_positive++;
  }

  // 误判率 = false_positive / 10000，应 < 2%
  double fp_rate = static_cast<double>(false_positive) / 10000.0;
  return fp_rate < 0.02;
}

// test_parameters - 验证参数计算合理性
bool test_parameters() {
  BloomFilter bf(1000, 0.01);

  // bit_count 应远大于元素数量
  if (bf.bit_count() < 1000)
    return false;

  // hash_count 应在合理范围（通常 3-15）
  if (bf.hash_count() < 1 || bf.hash_count() > 20)
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
      {"test_empty_filter", test_empty_filter},
      {"test_add_and_query", test_add_and_query},
      {"test_nonexistent_key", test_nonexistent_key},
      {"test_add_all", test_add_all},
      {"test_serialize_deserialize", test_serialize_deserialize},
      {"test_false_positive_rate", test_false_positive_rate},
      {"test_parameters", test_parameters},
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
