#pragma once

#include <filesystem>

namespace x1nglsm::utils {

/**
 * @brief 获取可执行文件的路径
 * @return 可执行文件的绝对路径
 */
std::filesystem::path get_executable_path();

/**
 * @brief 查找项目根目录（通过查找CMakeLists.txt）
 * @return 项目根目录路径
 */
std::filesystem::path find_project_root();

/**
 * @brief 设置控制台为UTF-8编码（跨平台）
 */
void set_utf8_encoding();

/**
 * @brief 初始化数据目录（处理相对路径，自动定位项目根目录）
 * @param data_dir 数据目录路径（输入输出参数）
 */
void initialize_data_directory(std::string &data_dir);

} // namespace x1nglsm::utils