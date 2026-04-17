cmake_minimum_required(VERSION 3.28)

project(X1ngLSM-KV VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17) # 设置语言标准为C++ 17
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 强制使用支持C++17的编译器，否则报错
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # 生成compile_commands.json，方便IDE

# 全局头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 子模块
add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(examples)

