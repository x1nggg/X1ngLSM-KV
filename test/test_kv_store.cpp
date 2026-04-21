#include "x1nglsm/kv_store.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

using namespace x1nglsm;

// ========== 测试辅助函数 ==========

// 清理测试目录
void cleanup_test_dir(const std::string &path) {
  std::filesystem::remove_all(path);
}

// ========== 测试用例 ==========

// test_put_get - 基本的Put和Get
bool test_put_get() {
  std::string test_dir = "./data/test/test_kv_basic";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);
    if (!store.put("key1", "value1"))
      return false;
    if (!store.put("key2", "value2"))
      return false;

    auto result1 = store.get("key1");
    if (!result1.has_value() || result1.value() != "value1")
      return false;

    auto result2 = store.get("key2");
    if (!result2.has_value() || result2.value() != "value2")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_put_override - 覆盖同一个key
bool test_put_override() {
  std::string test_dir = "./data/test/test_kv_override";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);
    store.put("key", "value1");
    store.put("key", "value2"); // 覆盖

    auto result = store.get("key");
    if (!result.has_value() || result.value() != "value2")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_remove - 删除操作
bool test_remove() {
  std::string test_dir = "./data/test/test_kv_remove";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);
    store.put("key", "value");
    store.remove("key");

    auto result = store.get("key");
    if (result.has_value())
      return false; // 删除后应该返回nullopt
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_nonexistent_key - 查询不存在的key
bool test_nonexistent_key() {
  std::string test_dir = "./data/test/test_kv_nonexistent";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);
    auto result = store.get("nonexistent");
    if (result.has_value())
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_crash_recovery - 崩溃恢复
bool test_crash_recovery() {
  std::string test_dir = "./data/test/test_kv_recovery";
  cleanup_test_dir(test_dir);

  // 第一阶段：写入数据
  {
    KVStore store(test_dir);
    store.put("key1", "value1");
    store.put("key2", "value2");
    store.put("key3", "value3");
  } // 离开作用域，KVStore析构

  // 第二阶段：重新打开，验证数据恢复
  {
    KVStore store(test_dir);
    auto result1 = store.get("key1");
    auto result2 = store.get("key2");
    auto result3 = store.get("key3");

    if (!result1.has_value() || result1.value() != "value1")
      return false;
    if (!result2.has_value() || result2.value() != "value2")
      return false;
    if (!result3.has_value() || result3.value() != "value3")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_empty_value - 空值处理
bool test_empty_value() {
  std::string test_dir = "./data/test/test_kv_empty_value";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);
    store.put("key", ""); // 空值

    auto result = store.get("key");
    if (!result.has_value())
      return false;
    if (result.value() != "")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_multiple_operations - 多个操作混合
bool test_multiple_operations() {
  std::string test_dir = "./data/test/test_kv_multiple";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);

    store.put("key1", "value1");
    store.put("key2", "value2");
    store.remove("key1");
    store.put("key3", "value3");
    store.put("key2", "value2_new"); // 覆盖

    auto result1 = store.get("key1"); // 应该被删除
    auto result2 = store.get("key2");
    auto result3 = store.get("key3");

    if (result1.has_value())
      return false;
    if (!result2.has_value() || result2.value() != "value2_new")
      return false;
    if (!result3.has_value() || result3.value() != "value3")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_remove_nonexistent - 删除不存在的key
bool test_remove_nonexistent() {
  std::string test_dir = "./data/test/test_kv_remove_nonexistent";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);
    // 删除不存在的key应该成功（幂等操作）
    if (!store.remove("nonexistent"))
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_flush - 测试 Flush 功能
bool test_flush() {
  std::string test_dir = "./data/test/test_kv_flush";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);

    // 写入大值数据触发 Flush（MemTable → Immutable → SSTable）
    std::string large_value(5 * 1024 * 1024, 'x'); // 5MB

    // 写入 7 次，总共 35MB，应该触发 Flush
    for (int i = 0; i < 7; i++) {
      std::string key = "key" + std::to_string(i);
      store.put(key, large_value);
    }

    // 验证数据仍然可以读取
    for (int i = 0; i < 7; i++) {
      std::string key = "key" + std::to_string(i);
      auto result = store.get(key);
      if (!result.has_value() || result.value() != large_value) {
        return false;
      }
    }
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_flush_and_recover - 测试 Flush 后重启恢复
bool test_flush_and_recover() {
  std::string test_dir = "./data/test/test_kv_flush_recover";
  cleanup_test_dir(test_dir);

  std::string large_value(5 * 1024 * 1024, 'x'); // 5MB

  // 第一阶段：写入并触发 Flush
  {
    KVStore store(test_dir);

    // 写入 7 次触发 Flush
    for (int i = 0; i < 7; i++) {
      std::string key = "key" + std::to_string(i);
      store.put(key, large_value);
    }
  }

  // 第二阶段：重启后验证数据
  {
    KVStore store(test_dir);

    for (int i = 0; i < 7; i++) {
      std::string key = "key" + std::to_string(i);
      auto result = store.get(key);
      if (!result.has_value() || result.value() != large_value) {
        return false;
      }
    }
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_sstable_query - 测试 SSTable 查询
bool test_sstable_query() {
  std::string test_dir = "./data/test/test_sstable_query";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);

    // 写入数据
    store.put("key1", "value1");
    store.put("key2", "value2");
    store.put("key3", "value3");

    // 触发 Flush
    std::string large_value(5 * 1024 * 1024, 'x');
    for (int i = 0; i < 7; i++) {
      store.put("flush" + std::to_string(i), large_value);
    }

    // 验证 Flush 前的数据仍然可以查询
    auto result1 = store.get("key1");
    auto result2 = store.get("key2");
    auto result3 = store.get("key3");

    if (!result1.has_value() || result1.value() != "value1")
      return false;
    if (!result2.has_value() || result2.value() != "value2")
      return false;
    if (!result3.has_value() || result3.value() != "value3")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_keys_tombstone_filter - 测试 keys() 过滤墓碑
bool test_keys_tombstone_filter() {
  std::string test_dir = "./data/test/test_keys_tombstone";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir);

    // 写入 3 个 key
    store.put("key1", "value1");
    store.put("key2", "value2");
    store.put("key3", "value3");

    // 删除 key2
    store.remove("key2");

    // keys() 不应包含已删除的 key
    auto all_keys = store.keys();
    for (const auto &key : all_keys) {
      if (key == "key2")
        return false;
    }
    // 应该恰好有 2 个 key
    if (all_keys.size() != 2)
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_keys_cross_sstable_delete - 测试跨 SSTable 删除后 keys() 正确
bool test_keys_cross_sstable_delete() {
  std::string test_dir = "./data/test/test_keys_cross_sstable";
  cleanup_test_dir(test_dir);

  std::string large_value(5 * 1024 * 1024, 'x');

  // 第一阶段：写入数据并 flush 到 SSTable
  {
    KVStore store(test_dir);
    store.put("alpha", "a");
    store.put("beta", "b");
    store.put("gamma", "c");

    // 触发 Flush（写入 7 个大值）
    for (int i = 0; i < 7; i++) {
      store.put("filler" + std::to_string(i), large_value);
    }
    // alpha, beta, gamma 现在在 SSTable 中
  }

  // 第二阶段：重新打开，删除 beta（写入新 SSTable），验证 keys() 不含 beta
  {
    KVStore store(test_dir);
    store.remove("beta");

    auto all_keys = store.keys();
    for (const auto &key : all_keys) {
      if (key == "beta")
        return false;
    }
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_get_cross_sstable_tombstone - 测试跨 SSTable 删除后 get() 返回 nullopt
bool test_get_cross_sstable_tombstone() {
  std::string test_dir = "./data/test/test_get_cross_sstable_tomb";
  cleanup_test_dir(test_dir);

  std::string large_value(5 * 1024 * 1024, 'x');

  // 第一阶段：写入并 flush 到 SSTable
  {
    KVStore store(test_dir);
    store.put("target", "old_value");
    for (int i = 0; i < 7; i++) {
      store.put("filler" + std::to_string(i), large_value);
    }
  }

  // 第二阶段：重新打开，删除 target，get 应返回 nullopt
  {
    KVStore store(test_dir);
    store.remove("target");

    auto result = store.get("target");
    if (result.has_value())
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_recovery_timestamp_correctness - 测试 WAL 恢复后时间戳正确
bool test_recovery_timestamp_correctness() {
  std::string test_dir = "./data/test/test_recovery_timestamp";
  cleanup_test_dir(test_dir);

  // 第一阶段：写入数据（不触发 flush，数据留在 WAL）
  {
    KVStore store(test_dir);
    store.put("key1", "value1");
    store.put("key2", "value2");
  }

  // 第二阶段：重新打开，恢复后写入新数据，旧数据不应被覆盖
  {
    KVStore store(test_dir);
    // 恢复后写入新 key
    store.put("key3", "value3");

    // 三个 key 都应该能正确读取
    auto r1 = store.get("key1");
    auto r2 = store.get("key2");
    auto r3 = store.get("key3");

    if (!r1.has_value() || r1.value() != "value1")
      return false;
    if (!r2.has_value() || r2.value() != "value2")
      return false;
    if (!r3.has_value() || r3.value() != "value3")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_immutable_flush - 测试 Immutable MemTable flush 后状态正确
bool test_immutable_flush() {
  std::string test_dir = "./data/test/test_immutable_flush";
  cleanup_test_dir(test_dir);

  {
    // 使用小阈值（1KB），方便触发 flush
    KVStore store(test_dir, 1024);

    // 写入几条数据触发 flush（MemTable → Immutable → SSTable）
    store.put("key1", "value1");
    store.put("key2", "value2");
    store.put("key3", std::string(1024, 'x')); // 触发 flush

    // flush 完成后，数据应全部可读（在 SSTable 中）
    auto r1 = store.get("key1");
    auto r2 = store.get("key2");
    auto r3 = store.get("key3");
    if (!r1.has_value() || r1.value() != "value1")
      return false;
    if (!r2.has_value() || r2.value() != "value2")
      return false;
    if (!r3.has_value() || r3.value() != std::string(1024, 'x'))
      return false;

    // SSTable 数量应为 1
    if (store.sstables_count() != 1)
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_immutable_and_active - 测试 flush 后新数据写入活跃
// MemTable，新旧数据均可读
bool test_immutable_and_active() {
  std::string test_dir = "./data/test/test_immutable_active";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir, 1024);

    // 第一轮：写入触发 flush（数据进入 SSTable）
    store.put("old_key1", "value1");
    store.put("old_key2", "value2");
    store.put("old_key3", std::string(1024, 'x')); // 触发 flush

    // 第二轮：继续写入新数据（在活跃 MemTable 中）
    store.put("new_key1", "new_value1");
    store.put("new_key2", "new_value2");

    // 新旧数据都应可读
    auto r1 = store.get("old_key1");
    auto r2 = store.get("old_key2");
    auto r3 = store.get("old_key3");
    if (!r1.has_value() || r1.value() != "value1")
      return false;
    if (!r2.has_value() || r2.value() != "value2")
      return false;
    if (!r3.has_value() || r3.value() != std::string(1024, 'x'))
      return false;

    auto r4 = store.get("new_key1");
    auto r5 = store.get("new_key2");
    if (!r4.has_value() || r4.value() != "new_value1")
      return false;
    if (!r5.has_value() || r5.value() != "new_value2")
      return false;

    // keys() 应包含所有有效 key
    auto all_keys = store.keys();
    if (all_keys.size() != 5)
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_sstable_compression - 测试 LZ4 压缩后数据正确读取
bool test_sstable_compression() {
  std::string test_dir = "./data/test/test_sstable_compression";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir, 1024);

    // 写入触发 flush，数据进入压缩的 SSTable
    store.put("apple", "red");
    store.put("banana", "yellow");
    store.put("cherry", std::string(1024, 'c')); // 触发 flush

    // 验证压缩后的 SSTable 能正确读取
    auto r1 = store.get("apple");
    auto r2 = store.get("banana");
    auto r3 = store.get("cherry");
    if (!r1.has_value() || r1.value() != "red")
      return false;
    if (!r2.has_value() || r2.value() != "yellow")
      return false;
    if (!r3.has_value() || r3.value() != std::string(1024, 'c'))
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_compression_ratio - 测试 LZ4 对重复数据的压缩效果
bool test_compression_ratio() {
  std::string test_dir = "./data/test/test_compression_ratio";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir, 1024);

    // 写入大量重复数据（LZ4 对重复数据压缩效果最好）
    std::string repeat_value(512, 'A'); // 512 字节全 A
    for (int i = 0; i < 10; i++) {
      store.put("key" + std::to_string(i), repeat_value);
    }
    // 触发 flush
    store.put("trigger", std::string(1024, 'T'));

    // 验证数据可读
    for (int i = 0; i < 10; i++) {
      auto r = store.get("key" + std::to_string(i));
      if (!r.has_value() || r.value() != repeat_value)
        return false;
    }

    // 检查 SSTable 文件大小：原始数据约 5KB+，压缩后应明显更小
    if (store.sstables_count() < 1)
      return false;

    std::string sst_dir = test_dir + "/level_0";
    for (const auto &entry : std::filesystem::directory_iterator(sst_dir)) {
      if (entry.path().extension() == ".sst") {
        auto file_size = std::filesystem::file_size(entry.path());
        // 索引区 + Bloom Filter + Footer 约 1KB，数据区原始 ~6KB
        // 压缩后整个文件应远小于原始数据大小（6KB）
        if (file_size >= 6 * 1024)
          return false;
      }
    }
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_compression_large_data - 测试大 value 压缩/解压正确性
bool test_compression_large_data() {
  std::string test_dir = "./data/test/test_compress_large";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir, 4096);

    // 写入多种不同 pattern 的大 value
    std::string pattern_a(2048, 'A');
    std::string pattern_b(2048, 'B');
    std::string pattern_c(2048, 'C');

    store.put("pa", pattern_a);
    store.put("pb", pattern_b);
    store.put("pc", pattern_c);

    // 触发 flush
    store.put("trigger", std::string(4096, 'X'));

    // 逐个验证解压正确性
    auto ra = store.get("pa");
    auto rb = store.get("pb");
    auto rc = store.get("pc");
    if (!ra.has_value() || ra.value() != pattern_a)
      return false;
    if (!rb.has_value() || rb.value() != pattern_b)
      return false;
    if (!rc.has_value() || rc.value() != pattern_c)
      return false;

    // 验证 trigger 本身
    auto rt = store.get("trigger");
    if (!rt.has_value() || rt.value() != std::string(4096, 'X'))
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_sstable_checksum - 测试 CRC32 校验能检测到文件损坏
bool test_sstable_checksum() {
  std::string test_dir = "./data/test/test_checksum";
  cleanup_test_dir(test_dir);

  {
    // 写入数据并触发 flush，产生 SSTable 文件
    KVStore store(test_dir, 256);
    store.put("key1", "value1");
    store.put("key2", std::string(256, 'A')); // 触发 flush

    // 正常读取验证
    auto r1 = store.get("key1");
    if (!r1.has_value() || r1.value() != "value1")
      return false;
    auto r2 = store.get("key2");
    if (!r2.has_value() || r2.value() != std::string(256, 'A'))
      return false;
  }

  // 篡改 SSTable 文件中间的一个字节
  std::string sst_dir = test_dir + "/level_0";
  for (const auto &entry : std::filesystem::directory_iterator(sst_dir)) {
    if (entry.path().extension() == ".sst") {
      std::string sst_path = entry.path().string();
      // 打开文件，篡改压缩数据区中间的一个字节
      std::fstream file(sst_path,
                        std::ios::in | std::ios::out | std::ios::binary);
      if (!file)
        return false;
      // 跳过 [原始大小(4)][压缩大小(4)]，篡改压缩数据的第 10 个字节
      file.seekp(18);
      char byte;
      file.read(&byte, 1);
      byte ^= 0xFF; // 翻转所有位
      file.seekp(18);
      file.write(&byte, 1);
      file.close();
    }
  }

  // 重新打开 store，尝试读取被篡改的数据
  // 注意：以下 get 调用会触发 checksum 校验失败，输出 Warning 是预期行为
  {
    KVStore store(test_dir);
    auto r1 = store.get("key1");
    if (r1.has_value())
      return false; // 应该读不到
    auto r2 = store.get("key2");
    if (r2.has_value())
      return false; // 应该读不到
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_compaction_basic - 测试 Level 0 SSTable 达到阈值时触发 compaction
bool test_compaction_basic() {
  std::string test_dir = "./data/test/test_compaction_basic";
  cleanup_test_dir(test_dir);

  {
    // 小阈值触发 flush，4 个 SSTable 触发 compaction
    KVStore store(test_dir, 256);

    // 写入 4 轮，每轮触发一次 flush，产生 4 个 Level 0 SSTable
    for (int round = 0; round < 4; round++) {
      store.put("key" + std::to_string(round), "value" + std::to_string(round));
      store.put("trigger" + std::to_string(round), std::string(256, 'T'));
    }

    // compaction 触发后，验证数据仍可读
    for (int i = 0; i < 4; i++) {
      auto r = store.get("key" + std::to_string(i));
      if (!r.has_value() || r.value() != "value" + std::to_string(i))
        return false;
    }
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_compaction_tombstone_cleanup - 测试 compaction 清理墓碑
bool test_compaction_tombstone_cleanup() {
  std::string test_dir = "./data/test/test_compaction_tomb";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir, 256);

    // 写入数据并 flush
    store.put("key1", "value1");
    store.put("trigger", std::string(256, 'T')); // flush 1

    // 删除 key1 并 flush
    store.remove("key1");
    store.put("trigger", std::string(256, 'T')); // flush 2

    store.put("key2", "value2");
    store.put("trigger", std::string(256, 'T')); // flush 3

    store.put("key3", "value3");
    store.put("trigger", std::string(256, 'T')); // flush 4 → compaction

    // key1 已删除，应该读不到
    auto r1 = store.get("key1");
    if (r1.has_value())
      return false;

    // 其他 key 应该可读
    auto r2 = store.get("key2");
    if (!r2.has_value() || r2.value() != "value2")
      return false;
  }

  cleanup_test_dir(test_dir);
  return true;
}

// test_compaction_recovery - 测试 compaction 后重启数据不丢失
bool test_compaction_recovery() {
  std::string test_dir = "./data/test/test_compaction_recovery";
  cleanup_test_dir(test_dir);

  {
    KVStore store(test_dir, 256);
    for (int round = 0; round < 4; round++) {
      store.put("key" + std::to_string(round), "value" + std::to_string(round));
      store.put("trigger" + std::to_string(round), std::string(256, 'T'));
    }
  }

  // 重启后验证
  {
    KVStore store(test_dir);
    for (int i = 0; i < 4; i++) {
      auto r = store.get("key" + std::to_string(i));
      if (!r.has_value() || r.value() != "value" + std::to_string(i))
        return false;
    }
  }

  cleanup_test_dir(test_dir);
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
      {"test_put_get", test_put_get},
      {"test_put_override", test_put_override},
      {"test_remove", test_remove},
      {"test_nonexistent_key", test_nonexistent_key},
      {"test_crash_recovery", test_crash_recovery},
      {"test_empty_value", test_empty_value},
      {"test_multiple_operations", test_multiple_operations},
      {"test_remove_nonexistent", test_remove_nonexistent},
      {"test_flush", test_flush},
      {"test_flush_and_recover", test_flush_and_recover},
      {"test_sstable_query", test_sstable_query},
      {"test_keys_tombstone_filter", test_keys_tombstone_filter},
      {"test_keys_cross_sstable_delete", test_keys_cross_sstable_delete},
      {"test_get_cross_sstable_tombstone", test_get_cross_sstable_tombstone},
      {"test_recovery_timestamp_correctness",
       test_recovery_timestamp_correctness},
      {"test_immutable_flush", test_immutable_flush},
      {"test_immutable_and_active", test_immutable_and_active},
      {"test_sstable_compression", test_sstable_compression},
      {"test_compression_ratio", test_compression_ratio},
      {"test_compression_large_data", test_compression_large_data},
      {"test_sstable_checksum", test_sstable_checksum},
      {"test_compaction_basic", test_compaction_basic},
      {"test_compaction_tombstone_cleanup", test_compaction_tombstone_cleanup},
      {"test_compaction_recovery", test_compaction_recovery},
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

  // 清理所有测试目录
  cleanup_test_dir("./data/test/test_kv_basic");
  cleanup_test_dir("./data/test/test_kv_override");
  cleanup_test_dir("./data/test/test_kv_remove");
  cleanup_test_dir("./data/test/test_kv_nonexistent");
  cleanup_test_dir("./data/test/test_kv_recovery");
  cleanup_test_dir("./data/test/test_kv_empty_value");
  cleanup_test_dir("./data/test/test_kv_multiple");
  cleanup_test_dir("./data/test/test_kv_remove_nonexistent");
  cleanup_test_dir("./data/test/test_kv_flush");
  cleanup_test_dir("./data/test/test_kv_flush_recover");
  cleanup_test_dir("./data/test/test_sstable_query");
  cleanup_test_dir("./data/test/test_keys_tombstone");
  cleanup_test_dir("./data/test/test_keys_cross_sstable");
  cleanup_test_dir("./data/test/test_get_cross_sstable_tomb");
  cleanup_test_dir("./data/test/test_recovery_timestamp");
  cleanup_test_dir("./data/test/test_immutable_flush");
  cleanup_test_dir("./data/test/test_immutable_active");
  cleanup_test_dir("./data/test/test_sstable_compression");
  cleanup_test_dir("./data/test/test_compression_ratio");
  cleanup_test_dir("./data/test/test_compress_large");
  cleanup_test_dir("./data/test/test_checksum");
  cleanup_test_dir("./data/test/test_compaction_basic");
  cleanup_test_dir("./data/test/test_compaction_tomb");
  cleanup_test_dir("./data/test/test_compaction_recovery");

  return (passed == total) ? 0 : 1;
}
