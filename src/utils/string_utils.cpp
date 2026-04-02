#include "x1nglsm/utils/string_utils.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace x1nglsm::utils {

std::string to_upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return s;
}

std::string format_size(size_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB"};
  int unit_index = 0;
  auto size = static_cast<double>(bytes);

  while (size >= 1024.0 && unit_index < 3) {
    size /= 1024.0;
    unit_index++;
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
  return oss.str();
}

} // namespace x1nglsm::utils