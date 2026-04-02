#include "x1nglsm/core/entry.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace x1nglsm::core;

// ========== 测试用例 ==========

// test_encode_put - 测试PUT类型Entry的序列化
bool test_encode_put() {
  Entry entry;
  entry.key = "name";
  entry.value = "alice";
  entry.type = OpType::PUT;
  entry.timestamp = 12345;

  std::string data = entry.encode();

  // 验证长度
  size_t expected_len = 17 + entry.key.size() + entry.value.size();
  if (data.size() != expected_len)
    return false;

  // 验证第一字节是 PUT (0)
  if (static_cast<uint8_t>(data[0]) != 0)
    return false;

  return true;
}

// test_encode_delete - 测试DELETE类型Entry的序列化
bool test_encode_delete() {
  Entry entry;
  entry.key = "name";
  entry.value = "";
  entry.type = OpType::DELETE;
  entry.timestamp = 12346;

  std::string data = entry.encode();

  // 验证第一字节是 DELETE (1)
  if (static_cast<uint8_t>(data[0]) != 1)
    return false;

  return true;
}

// test_decode_put - 测试PUT类型Entry的反序列化
bool test_decode_put() {
  // 手动构造一个序列化数据
  std::string data;
  data.push_back(static_cast<char>(OpType::PUT)); // type

  uint64_t ts = 12345;
  data.append(reinterpret_cast<char *>(&ts), 8); // timestamp

  uint32_t klen = 4; // "name"
  data.append(reinterpret_cast<char *>(&klen), 4);
  data.append("name");

  uint32_t vlen = 5; // "alice"
  data.append(reinterpret_cast<char *>(&vlen), 4);
  data.append("alice");

  auto result = Entry::decode(data);
  if (!result.has_value())
    return false;

  Entry entry = result.value();
  if (entry.key != "name")
    return false;
  if (entry.value != "alice")
    return false;
  if (entry.timestamp != 12345)
    return false;
  if (entry.type != OpType::PUT)
    return false;

  return true;
}

// test_round_trip - 测试序列化后反序列化的一致性
bool test_round_trip() {
  // 原始Entry
  Entry original;
  original.key = "test_key";
  original.value = "test_value_with_more_data";
  original.type = OpType::PUT;
  original.timestamp = 99999;

  // 序列化 -> 反序列化
  std::string data = original.encode();
  auto result = Entry::decode(data);

  if (!result.has_value())
    return false;

  Entry decoded = result.value();
  if (decoded.key != original.key)
    return false;
  if (decoded.value != original.value)
    return false;
  if (decoded.timestamp != original.timestamp)
    return false;
  if (decoded.type != original.type)
    return false;

  return true;
}

// test_tombstone - 测试墓碑标记
bool test_tombstone() {
  Entry entry;
  entry.key = "deleted_key";
  entry.value = "";
  entry.type = OpType::DELETE;
  entry.timestamp = 1;

  if (entry.is_tombstone() != true)
    return false;

  // 序列化后反序列化，墓碑属性应该保持
  std::string data = entry.encode();
  auto result = Entry::decode(data);
  if (!result.has_value())
    return false;
  if (result->is_tombstone() != true)
    return false;

  return true;
}

// test_empty_value - 测试空value
bool test_empty_value() {
  // 空value（合法）
  Entry e1;
  e1.key = "key";
  e1.value = "";
  e1.type = OpType::PUT;
  e1.timestamp = 1;

  auto data1 = e1.encode();
  auto r1 = Entry::decode(data1);
  if (!r1.has_value())
    return false;

  return true;
}

// test_decode_invalid_data - 测试非法数据处理
bool test_decode_invalid_data() {
  // 空数据
  auto r1 = Entry::decode("");
  if (r1.has_value())
    return false;

  // 不完整数据（只有type）
  std::string data2;
  data2.push_back(static_cast<char>(OpType::PUT));
  auto r2 = Entry::decode(data2);
  if (r2.has_value())
    return false;

  return true;
}

// test_size_calculation - 测试encode_size()计算
bool test_size_calculation() {
  Entry entry;
  entry.key = "key";     // 3字节
  entry.value = "value"; // 5字节
  entry.type = OpType::PUT;
  entry.timestamp = 1;

  // 17 + 3 + 5 = 25
  if (entry.encode_size() != 25)
    return false;
  if (entry.encode_size() != entry.encode().size())
    return false;

  return true;
}

// ========== 主函数 ==========

int main() {
  int passed = 0;
  int total = 0;

  // 测试用例数组
  struct TestCase {
    const char *name;
    bool (*func)();
  };

  TestCase tests[] = {
      {"test_encode_put", test_encode_put},
      {"test_encode_delete", test_encode_delete},
      {"test_decode_put", test_decode_put},
      {"test_round_trip", test_round_trip},
      {"test_tombstone", test_tombstone},
      {"test_empty_value", test_empty_value},
      {"test_decode_invalid_data", test_decode_invalid_data},
      {"test_size_calculation", test_size_calculation},
  };

  int num_tests = sizeof(tests) / sizeof(tests[0]);

  // 运行所有测试
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
