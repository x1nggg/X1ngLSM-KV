#pragma once

#include <string>

namespace x1nglsm::utils {

/**
 * @brief Glob 风格的模式匹配
 * @param pattern 模式字符串，支持:
 *                 * - 匹配0个或多个字符
 *                 ? - 匹配单个字符
 * @param text 要匹配的文本
 * @return 是否匹配
 *
 * @example
 *   glob_match("user:*", "user:123")  -> true
 *   glob_match("user:?", "user:a")    -> true
 *   glob_match("user:?", "user:ab")   -> false
 *   glob_match("*", "anything")       -> true
 */
bool glob_match(const std::string &pattern, const std::string &text);

} // namespace x1nglsm::utils
