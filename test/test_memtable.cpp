#include "x1nglsm/core/mem_table.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace x1nglsm::core;

// 测试辅助类（用于访问private成员）
namespace x1nglsm::core {
    class MemTableTest {
    public:
        static std::vector<Entry> get_all_entries(const MemTable &mt) {
            return mt.get_all_entries();
        }

        static void clear(MemTable &mt) {
            mt.clear();
        }
    };
}

// ========== 测试用例 ==========

// test_put_then_get - Put后能Get到值
bool test_put_then_get() {
    MemTable mt;
    mt.put("key1", "value1");

    auto result = mt.get("key1");
    return result.has_value() && result.value() == "value1";
}

// test_put_multiple - 多次Put不同key
bool test_put_multiple() {
    MemTable mt;
    mt.put("a", "1");
    mt.put("b", "2");
    mt.put("c", "3");

    return mt.get("a").value() == "1" &&
           mt.get("b").value() == "2" &&
           mt.get("c").value() == "3";
}

// test_update_existing - 更新已存在的key
bool test_update_existing() {
    MemTable mt;
    mt.put("key", "old_value");
    mt.put("key", "new_value");  // 覆盖

    return mt.get("key").value() == "new_value" &&
           mt.num_entries() == 1;
}

// test_delete_then_get - Delete后Get返回nullopt
bool test_delete_then_get() {
    MemTable mt;
    mt.put("key", "value");
    mt.remove("key");

    return !mt.get("key").has_value();
}

// test_delete_nonexist - 删除不存在的key写入墓碑
bool test_delete_nonexist() {
    MemTable mt;
    mt.remove("nonexist");  // 应该写入墓碑

    // 墓碑也是Entry，num_entries应该为1
    return mt.num_entries() == 1 &&
           !mt.get("nonexist").has_value();  // 但get返回nullopt
}

// test_put_after_delete - 删除后再Put
bool test_put_after_delete() {
    MemTable mt;
    mt.put("key", "value1");
    mt.remove("key");
    mt.put("key", "value2");

    return mt.get("key").value() == "value2" &&
           mt.num_entries() == 1;
}

// test_get_nonexist - 查询不存在的key返回nullopt
bool test_get_nonexist() {
    MemTable mt;
    mt.put("other", "value");

    return !mt.get("nonexist").has_value();
}

// test_get_empty_table - 空表查询返回nullopt
bool test_get_empty_table() {
    MemTable mt;
    return !mt.get("anything").has_value();
}

// test_size_empty - 空表大小为0
bool test_size_empty() {
    MemTable mt;
    return mt.num_entries() == 0 &&
           mt.total_encoded_size() == 0;
}

// test_size_after_put - Put后大小增加
bool test_size_after_put() {
    MemTable mt;
    mt.put("key", "value");

    // Entry.encode_size() = 17 + klen + vlen = 17 + 3 + 5 = 25
    size_t expected = 17 + std::string("key").size() + std::string("value").size();

    return mt.num_entries() == 1 &&
           mt.total_encoded_size() == expected;
}

// test_size_after_update - 更新后大小计算正确
bool test_size_after_update() {
    MemTable mt;
    mt.put("key", "short");      // 17 + 3 + 5 = 25
    size_t size1 = mt.total_encoded_size();

    mt.put("key", "longer_value");  // 17 + 3 + 12 = 32
    size_t size2 = mt.total_encoded_size();

    return size2 == 32 &&
           mt.num_entries() == 1;
}

// test_size_after_delete - Delete后大小变化
bool test_size_after_delete() {
    MemTable mt;
    mt.put("key", "value");  // 17 + 3 + 5 = 25

    mt.remove("key");  // 墓碑: 17 + 3 + 0 = 20
    size_t size2 = mt.total_encoded_size();

    return size2 == 20 &&
           mt.num_entries() == 1;
}

// test_empty - 判断空表
bool test_empty() {
    MemTable mt;
    return mt.empty();
}

// test_not_empty - 非空表
bool test_not_empty() {
    MemTable mt;
    mt.put("key", "value");
    return !mt.empty();
}

// test_num_entries - Entry数量正确
bool test_num_entries() {
    MemTable mt;
    mt.put("a", "1");
    mt.put("b", "2");
    mt.remove("c");  // 墓碑也是Entry

    return mt.num_entries() == 3;
}

// test_get_all_entries_empty - 空表返回空vector
bool test_get_all_entries_empty() {
    MemTable mt;
    auto entries = MemTableTest::get_all_entries(mt);
    return entries.empty();
}

// test_get_all_entries_sorted - 返回的Entry按key排序
bool test_get_all_entries_sorted() {
    MemTable mt;
    mt.put("c", "3");
    mt.put("a", "1");
    mt.put("b", "2");

    auto entries = MemTableTest::get_all_entries(mt);

    // 检查是否按key排序: a, b, c
    return entries.size() == 3 &&
           entries[0].key == "a" &&
           entries[1].key == "b" &&
           entries[2].key == "c";
}

// test_get_all_entries_with_tombstone - 包含墓碑Entry
bool test_get_all_entries_with_tombstone() {
    MemTable mt;
    mt.put("key", "value");
    mt.remove("key");

    auto entries = MemTableTest::get_all_entries(mt);

    // 应该有1个Entry（DELETE墓碑覆盖了PUT）
    // 注意：std::map中key唯一，新的Entry会覆盖旧的
    return entries.size() == 1 && entries[0].is_tombstone();
}

// test_timestamp_increasing - 时间戳严格递增
bool test_timestamp_increasing() {
    MemTable mt;
    mt.put("a", "1");
    mt.put("b", "2");
    mt.remove("c");

    auto entries = MemTableTest::get_all_entries(mt);

    // 检查时间戳是否递增
    return entries.size() == 3 &&
           entries[0].timestamp < entries[1].timestamp &&
           entries[1].timestamp < entries[2].timestamp;
}

// test_advance_timestamp - 测试时间戳推进
bool test_advance_timestamp() {
    MemTable mt;

    // 初始时间戳从 1 开始
    mt.put("a", "1");  // timestamp 1
    mt.put("b", "2");  // timestamp 2

    // 推进到 100
    mt.advance_timestamp(100);

    // 下一个时间戳应该从 100 开始
    uint64_t ts = mt.get_next_timestamp();
    if (ts != 100) return false;

    // 推进后写入应使用新的时间戳
    mt.put("c", "3");  // timestamp 101
    auto entries = MemTableTest::get_all_entries(mt);
    if (entries[2].timestamp != 101) return false;

    // 推进到更小的值不应降低时间戳
    mt.advance_timestamp(50);
    ts = mt.get_next_timestamp();
    if (ts != 102) return false;

    return true;
}

// test_clear - 清空后状态重置
bool test_clear() {
    MemTable mt;
    mt.put("a", "1");
    mt.put("b", "2");

    MemTableTest::clear(mt);

    return mt.empty() &&
           mt.num_entries() == 0 &&
           mt.total_encoded_size() == 0;
}

// test_empty_key_value - 测试空key和value
bool test_empty_key_value() {
    MemTable mt;
    mt.put("", "value");   // 空key
    mt.put("key", "");     // 空value

    return mt.num_entries() == 2 &&
           mt.get("").has_value() &&
           mt.get("key").has_value();
}

// test_long_key_value - 测试长key和value
bool test_long_key_value() {
    MemTable mt;
    std::string long_key(1000, 'a');
    std::string long_value(10000, 'b');

    mt.put(long_key, long_value);

    auto result = mt.get(long_key);
    return result.has_value() &&
           result.value() == long_value;
}

// test_special_characters - 测试特殊字符
bool test_special_characters() {
    MemTable mt;
    mt.put("key\n", "value\r");
    mt.put("key\t", "value\x00");

    return mt.get("key\n").has_value() &&
           mt.get("key\t").has_value();
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
            // 基础Put/Get测试
            {"test_put_then_get",                   test_put_then_get},
            {"test_put_multiple",                   test_put_multiple},
            {"test_update_existing",                test_update_existing},

            // Delete测试
            {"test_delete_then_get",                test_delete_then_get},
            {"test_delete_nonexist",                test_delete_nonexist},
            {"test_put_after_delete",               test_put_after_delete},

            // Get测试
            {"test_get_nonexist",                   test_get_nonexist},
            {"test_get_empty_table",                test_get_empty_table},

            // 大小测试
            {"test_size_empty",                     test_size_empty},
            {"test_size_after_put",                 test_size_after_put},
            {"test_size_after_update",              test_size_after_update},
            {"test_size_after_delete",              test_size_after_delete},

            // 状态查询测试
            {"test_empty",                          test_empty},
            {"test_not_empty",                      test_not_empty},
            {"test_num_entries",                    test_num_entries},

            // get_all_entries测试
            {"test_get_all_entries_empty",          test_get_all_entries_empty},
            {"test_get_all_entries_sorted",         test_get_all_entries_sorted},
            {"test_get_all_entries_with_tombstone", test_get_all_entries_with_tombstone},

            // 时间戳测试
            {"test_timestamp_increasing",           test_timestamp_increasing},
            {"test_advance_timestamp",              test_advance_timestamp},

            // Clear测试
            {"test_clear",                          test_clear},

            // 边界情况测试
            {"test_empty_key_value",                test_empty_key_value},
            {"test_long_key_value",                 test_long_key_value},
            {"test_special_characters",             test_special_characters},
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
            std::cout << "[ERROR] " << tests[i].name << " - " << e.what() << std::endl;
        } catch (...) {
            std::cout << "[ERROR] " << tests[i].name << " (unknown exception)" << std::endl;
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
