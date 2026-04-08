# 设置示例程序的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/examples)

# 示例程序源文件
set(EXAMPLE_SOURCES
    ../src/core/entry.cpp
    ../src/core/mem_table.cpp
    ../src/core/write_ahead_log.cpp
    ../src/core/sstable.cpp
    ../src/core/bloom_filter.cpp
    ../src/kv_store.cpp
    ../src/utils/glob_utils.cpp
)

# 示例 1: 基础操作示例
add_executable(example_basic example_basic.cpp ${EXAMPLE_SOURCES})
target_include_directories(example_basic PRIVATE ${CMAKE_SOURCE_DIR}/include)

# 示例 2: 批量操作示例
add_executable(example_batch example_batch.cpp ${EXAMPLE_SOURCES})
target_include_directories(example_batch PRIVATE ${CMAKE_SOURCE_DIR}/include)

# 示例 3: 持久化和恢复示例
add_executable(example_persistence example_persistence.cpp ${EXAMPLE_SOURCES})
target_include_directories(example_persistence PRIVATE ${CMAKE_SOURCE_DIR}/include)

# 示例 4: 通配符查询示例
add_executable(example_wildcard example_wildcard.cpp ${EXAMPLE_SOURCES})
target_include_directories(example_wildcard PRIVATE ${CMAKE_SOURCE_DIR}/include)
