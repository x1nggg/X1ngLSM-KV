# LZ4 压缩库（静态库）
add_library(lz4 STATIC lz4/lz4.c)
target_include_directories(lz4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lz4)