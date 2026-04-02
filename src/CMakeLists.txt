set(CLI_SRC_LIST
        cli/cli.cpp
        cli/commands.cpp
        utils/string_utils.cpp
        utils/system_utils.cpp
        utils/arg_utils.cpp
        utils/glob_utils.cpp
        core/entry.cpp
        core/mem_table.cpp
        core/write_ahead_log.cpp
        core/sstable.cpp
        kv_store.cpp
)

# 设置CLI程序的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/cli)

# 编译主程序（cli/cli.cpp 是入口）
add_executable(x1nglsm-cli ${CLI_SRC_LIST})