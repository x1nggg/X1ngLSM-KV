# 设置测试程序的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/test)

# Entry模块测试
set(TEST_ENTRY_SRC_LIST test_entry.cpp ${PROJECT_SOURCE_DIR}/src/core/entry.cpp)
add_executable(test_entry ${TEST_ENTRY_SRC_LIST})

# MemTable模块测试
set(TEST_MEMTABLE_SRC_LIST test_memtable.cpp
    ${PROJECT_SOURCE_DIR}/src/core/entry.cpp
    ${PROJECT_SOURCE_DIR}/src/core/mem_table.cpp)
add_executable(test_memtable ${TEST_MEMTABLE_SRC_LIST})

# WAL模块测试
set(TEST_WAL_SRC_LIST test_wal.cpp
    ${PROJECT_SOURCE_DIR}/src/core/entry.cpp
    ${PROJECT_SOURCE_DIR}/src/core/write_ahead_log.cpp)
add_executable(test_wal ${TEST_WAL_SRC_LIST})

# KVStore模块测试
set(TEST_KVSTORE_SRC_LIST test_kv_store.cpp
    ${PROJECT_SOURCE_DIR}/src/core/entry.cpp
    ${PROJECT_SOURCE_DIR}/src/core/mem_table.cpp
    ${PROJECT_SOURCE_DIR}/src/core/write_ahead_log.cpp
    ${PROJECT_SOURCE_DIR}/src/core/sstable.cpp
    ${PROJECT_SOURCE_DIR}/src/core/bloom_filter.cpp
    ${PROJECT_SOURCE_DIR}/src/kv_store.cpp
    )
add_executable(test_kv_store ${TEST_KVSTORE_SRC_LIST})

# BloomFilter模块测试
set(TEST_BLOOM_FILTER_SRC_LIST test_bloom_filter.cpp
    ${PROJECT_SOURCE_DIR}/src/core/bloom_filter.cpp)
add_executable(test_bloom_filter ${TEST_BLOOM_FILTER_SRC_LIST})

target_include_directories(test_bloom_filter PRIVATE ${CMAKE_SOURCE_DIR}/include)
