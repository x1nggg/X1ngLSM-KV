#include "x1nglsm/utils/system_utils.hpp"
#include <filesystem>
#include <iostream>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#endif

namespace x1nglsm::utils {

std::filesystem::path get_executable_path() {
#ifdef _WIN32
  char buffer[MAX_PATH];
  GetModuleFileNameA(NULL, buffer, MAX_PATH);
  return std::filesystem::path(buffer);
#else
  // 非Windows平台：直接使用当前工作目录
  // （从项目根目录运行程序）
  return std::filesystem::current_path();
#endif
}

std::filesystem::path find_project_root() {
  std::filesystem::path exe_path = get_executable_path();
  std::filesystem::path current = exe_path.parent_path();

  // 最多向上查找5层
  for (int i = 0; i < 5; ++i) {
    if (std::filesystem::exists(current / "CMakeLists.txt")) {
      return current;
    }
    if (current.has_parent_path()) {
      current = current.parent_path();
    } else {
      break;
    }
  }

  // 如果找不到，返回当前工作目录
  return std::filesystem::current_path();
}

void set_utf8_encoding() {
#ifdef _WIN32
  SetConsoleOutputCP(65001); // CP_UTF8 = 65001
#else
  // Linux/Unix: 设置locale为UTF-8
  try {
    std::locale::global(std::locale("en_US.UTF-8"));
  } catch (...) {
    // 如果en_US.UTF-8不可用，尝试C.UTF-8
    try {
      std::locale::global(std::locale("C.UTF-8"));
    } catch (...) {
      // 如果都失败，使用系统默认locale
      std::locale::global(std::locale(""));
    }
  }
  std::cout.imbue(std::locale());
#endif
}

void initialize_data_directory(std::string &data_dir) {
  std::filesystem::path data_path(data_dir);
  if (!data_path.is_absolute()) {
    // 如果是相对路径，检查是否使用默认值
    const std::string default_dir = "./data/cli";
    if (data_dir == default_dir) {
      // 使用项目根目录
      auto project_root = find_project_root();
      data_path = project_root / "data" / "cli";
      data_dir = data_path.string();
    }
  }
}

} // namespace x1nglsm::utils