#include "x1nglsm/utils/glob_utils.hpp"

#include <cstdint>
#include <unordered_map>

namespace x1nglsm::utils {

/**
 * @brief Glob 风格的模式匹配（带 memoization 的递归实现）
 * @param p 模式字符串的当前位置
 * @param t 文本字符串的当前位置
 * @param pattern_end 模式字符串的结束位置
 * @param text_end 文本字符串的结束位置
 * @param cache 已计算结果的缓存，key 为 (pattern_pos, text_pos)
 * @return 是否匹配
 */
static bool glob_match_impl(const char *p, const char *t,
                            const char *pattern_end, const char *text_end,
                            std::unordered_map<uint64_t, bool> &cache) {
  // 如果模式已经用完
  if (p == pattern_end) {
    // 文本也用完才算匹配
    return t == text_end;
  }

  // 查缓存（用剩余未匹配长度作为 key，保证非负）
  uint64_t key =
      (static_cast<uint64_t>(static_cast<size_t>(pattern_end - p)) << 32) |
      static_cast<uint32_t>(text_end - t);
  auto it = cache.find(key);
  if (it != cache.end())
    return it->second;

  bool result = false;

  // 处理 * 通配符（匹配0个或多个字符）
  if (*p == '*') {
    // 跳过连续的 *
    while (p != pattern_end && *p == '*') {
      ++p;
    }

    // 如果模式只有 *，直接匹配
    if (p == pattern_end) {
      result = true;
    } else {
      // 尝试用 * 匹配0个、1个、2个...字符
      const char *tt = t;
      while (tt != text_end) {
        if (glob_match_impl(p, tt, pattern_end, text_end, cache)) {
          result = true;
          break;
        }
        ++tt;
      }

      // 尝试用 * 匹配剩余所有字符
      if (!result) {
        result = glob_match_impl(p, text_end, pattern_end, text_end, cache);
      }
    }
  }
  // 处理 ? 通配符（匹配单个字符）
  else if (*p == '?') {
    // 必须有一个字符可以匹配
    if (t != text_end) {
      result = glob_match_impl(p + 1, t + 1, pattern_end, text_end, cache);
    }
  }
  // 普通字符必须精确匹配
  else if (t != text_end && *p == *t) {
    result = glob_match_impl(p + 1, t + 1, pattern_end, text_end, cache);
  }

  cache[key] = result;
  return result;
}

bool glob_match(const std::string &pattern, const std::string &text) {
  std::unordered_map<uint64_t, bool> cache;
  return glob_match_impl(pattern.c_str(), text.c_str(),
                         pattern.c_str() + pattern.size(),
                         text.c_str() + text.size(), cache);
}

} // namespace x1nglsm::utils
