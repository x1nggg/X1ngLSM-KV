#pragma once

#include <string>

namespace x1nglsm::utils {

/**
 * @brief 解析命令行参数
 * @param argc 参数个数
 * @param argv 参数数组
 * @param out_dir 输出：解析后的数据目录路径
 */
void parse_args(int argc, char *argv[], std::string &out_dir);

} // namespace x1nglsm::utils