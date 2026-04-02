#pragma once

#include <string>

namespace x1nglsm::utils {

/**
 * @brief 转换字符串为大写
 * @param s 输入字符串
 * @return 大写字符串
 */
std::string to_upper(std::string s);

/**
 * @brief 格式化字节大小为人类可读格式
 * @param bytes 字节数
 * @return 格式化后的字符串（如 "1.50 KB"）
 */
std::string format_size(size_t bytes);

} // namespace x1nglsm::utils