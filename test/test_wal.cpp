#include "x1nglsm/core/write_ahead_log.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <filesystem>

using namespace x1nglsm::core;

// ========== 测试辅助函数 ==========

// 清理测试文件
void cleanup_test_file(const std::string &path) {
    std::remove(path.c_str());
}

// ========== 测试用例 ==========

// test_append_single - 追加单个Entry
bool test_append_single() {
    std::string test_path = "test_wal_single.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);
        Entry entry("key", "value", OpType::PUT, 1);
        bool result = wal.append(entry);
        if (!result) return false;
    }

    // 验证文件是否创建
    std::ifstream file(test_path, std::ios::binary);
    if (!file.is_open()) return false;

    cleanup_test_file(test_path);
    return true;
}

// test_append_multiple - 追加多个Entry
bool test_append_multiple() {
    std::string test_path = "test_wal_multiple.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);

        std::vector<Entry> entries;
        entries.emplace_back("key1", "value1", OpType::PUT, 1);
        entries.emplace_back("key2", "value2", OpType::PUT, 2);
        entries.emplace_back("key3", "value3", OpType::PUT, 3);

        bool result = wal.append(entries);
        if (!result) return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_read_all - 读取所有Entry
bool test_read_all() {
    std::string test_path = "test_wal_read.log";
    cleanup_test_file(test_path);

    // 先写入
    {
        WriteAheadLog wal(test_path);

        std::vector<Entry> entries;
        entries.emplace_back("key1", "value1", OpType::PUT, 1);
        entries.emplace_back("key2", "value2", OpType::PUT, 2);

        if (!wal.append(entries)) return false;
    }

    // 再读取
    {
        WriteAheadLog wal(test_path);
        auto read_entries = wal.read_all();

        if (read_entries.size() != 2) return false;
        if (read_entries[0].key != "key1") return false;
        if (read_entries[1].key != "key2") return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_round_trip - 写入后再读取，数据一致
bool test_round_trip() {
    std::string test_path = "test_wal_roundtrip.log";
    cleanup_test_file(test_path);

    std::vector<Entry> original_entries;
    original_entries.emplace_back("name", "Alice", OpType::PUT, 100);
    original_entries.emplace_back("age", "25", OpType::PUT, 101);
    original_entries.emplace_back("city", "Beijing", OpType::PUT, 102);
    original_entries.emplace_back("deleted", "", OpType::DELETE, 103);

    // 写入
    {
        WriteAheadLog wal(test_path);
        if (!wal.append(original_entries)) return false;
    }

    // 读取
    {
        WriteAheadLog wal(test_path);
        auto read_entries = wal.read_all();

        if (read_entries.size() != original_entries.size()) return false;

        for (size_t i = 0; i < read_entries.size(); i++) {
            if (read_entries[i].key != original_entries[i].key) return false;
            if (read_entries[i].value != original_entries[i].value) return false;
            if (read_entries[i].type != original_entries[i].type) return false;
            if (read_entries[i].timestamp != original_entries[i].timestamp) return false;
        }
    }

    cleanup_test_file(test_path);
    return true;
}

// test_empty_file - 空文件读取
bool test_empty_file() {
    std::string test_path = "test_wal_empty.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);
        // 不写入任何Entry
    }

    {
        WriteAheadLog wal(test_path);
        auto entries = wal.read_all();

        if (!entries.empty()) return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_nonexistent_file - 不存在的文件读取
bool test_nonexistent_file() {
    std::string test_path = "test_wal_nonexistent.log";
    cleanup_test_file(test_path);

    WriteAheadLog wal(test_path);
    auto entries = wal.read_all();

    // 文件不存在应该返回空列表
    if (entries.size() != 0) return false;

    return true;
}

// test_large_entry - 大Entry测试
bool test_large_entry() {
    std::string test_path = "test_wal_large.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);

        std::string large_key(1000, 'k');
        std::string large_value(10000, 'v');

        Entry entry(large_key, large_value, OpType::PUT, 1);
        if (!wal.append(entry)) return false;
    }

    {
        WriteAheadLog wal(test_path);
        auto entries = wal.read_all();

        if (entries.size() != 1) return false;
        if (entries[0].key.size() != 1000) return false;
        if (entries[0].value.size() != 10000) return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_append_empty - 测试空Entry列表
bool test_append_empty() {
    std::string test_path = "test_wal_empty_append.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);
        std::vector<Entry> empty_entries;
        if (!wal.append(empty_entries)) return false;  // 应该成功
    }

    {
        WriteAheadLog wal(test_path);
        auto entries = wal.read_all();
        if (entries.size() != 0) return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_delete_entry - 测试墓碑Entry
bool test_delete_entry() {
    std::string test_path = "test_wal_delete.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);

        Entry entry("key", "", OpType::DELETE, 1);
        if (!wal.append(entry)) return false;
    }

    {
        WriteAheadLog wal(test_path);
        auto entries = wal.read_all();

        if (entries.size() != 1) return false;
        if (!entries[0].is_tombstone()) return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_mixed_operations - 混合操作测试
bool test_mixed_operations() {
    std::string test_path = "test_wal_mixed.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);

        // 先追加单个
        Entry e1("key1", "value1", OpType::PUT, 1);
        if (!wal.append(e1)) return false;

        // 再追加多个
        std::vector<Entry> entries;
        entries.emplace_back("key2", "value2", OpType::PUT, 2);
        entries.emplace_back("key3", "value3", OpType::PUT, 3);
        if (!wal.append(entries)) return false;

        // 再追加单个
        Entry e2("key4", "value4", OpType::PUT, 4);
        if (!wal.append(e2)) return false;
    }

    {
        WriteAheadLog wal(test_path);
        auto entries = wal.read_all();

        if (entries.size() != 4) return false;
        if (entries[0].key != "key1") return false;
        if (entries[1].key != "key2") return false;
        if (entries[2].key != "key3") return false;
        if (entries[3].key != "key4") return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_crc_corruption - 篡改文件后CRC校验应失败，只返回完整记录
bool test_crc_corruption() {
    std::string test_path = "test_wal_crc.log";
    cleanup_test_file(test_path);

    // 写入 3 条记录
    {
        WriteAheadLog wal(test_path);
        wal.append(Entry("key1", "value1", OpType::PUT, 1));
        wal.append(Entry("key2", "value2", OpType::PUT, 2));
        wal.append(Entry("key3", "value3", OpType::PUT, 3));
    }

    // 篡改文件：在文件末尾追加垃圾数据（模拟崩溃时写入不完整）
    {
        std::ofstream file(test_path, std::ios::binary | std::ios::app);
        file.write("garbage_data", 13);
    }

    // 读取：前 3 条记录应该完好，损坏部分应被跳过
    {
        WriteAheadLog wal(test_path);
        auto entries = wal.read_all();

        if (entries.size() != 3) return false;
        if (entries[0].key != "key1") return false;
        if (entries[1].key != "key2") return false;
        if (entries[2].key != "key3") return false;
    }

    cleanup_test_file(test_path);
    return true;
}

// test_size_tracking - 写入后文件应增长
bool test_size_tracking() {
    std::string test_path = "test_wal_size.log";
    cleanup_test_file(test_path);

    {
        WriteAheadLog wal(test_path);
        wal.append(Entry("key1", "value1", OpType::PUT, 1));
    }

    // 写入后文件应有内容
    size_t size_after_one = std::filesystem::file_size(test_path);
    if (size_after_one == 0) return false;

    // 再写入一条，文件应更大
    {
        WriteAheadLog wal(test_path);
        wal.append(Entry("key2", "value2", OpType::PUT, 2));
    }

    size_t size_after_two = std::filesystem::file_size(test_path);
    if (size_after_two <= size_after_one) return false;

    cleanup_test_file(test_path);
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
            {"test_append_single",       test_append_single},
            {"test_append_multiple",     test_append_multiple},
            {"test_read_all",            test_read_all},
            {"test_round_trip",          test_round_trip},
            {"test_empty_file",          test_empty_file},
            {"test_nonexistent_file",    test_nonexistent_file},
            {"test_large_entry",         test_large_entry},
            {"test_append_empty",        test_append_empty},
            {"test_delete_entry",        test_delete_entry},
            {"test_mixed_operations",    test_mixed_operations},
            {"test_crc_corruption",      test_crc_corruption},
            {"test_size_tracking",       test_size_tracking},
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
            std::cout << "[ERROR] " << tests[i].name << " - " << e.what() << std::endl;
        } catch (...) {
            std::cout << "[ERROR] " << tests[i].name << " (unknown exception)" << std::endl;
        }
    }

    std::cout << "\n=================================" << std::endl;
    std::cout << "Passed: " << passed << "/" << total << std::endl;

    // 清理所有测试文件
    cleanup_test_file("test_wal_single.log");
    cleanup_test_file("test_wal_multiple.log");
    cleanup_test_file("test_wal_read.log");
    cleanup_test_file("test_wal_roundtrip.log");
    cleanup_test_file("test_wal_empty.log");
    cleanup_test_file("test_wal_nonexistent.log");
    cleanup_test_file("test_wal_large.log");
    cleanup_test_file("test_wal_empty_append.log");
    cleanup_test_file("test_wal_delete.log");
    cleanup_test_file("test_wal_mixed.log");
    cleanup_test_file("test_wal_crc.log");
    cleanup_test_file("test_wal_size.log");

    return (passed == total) ? 0 : 1;
}
