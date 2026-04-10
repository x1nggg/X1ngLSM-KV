#include "x1nglsm/cli/commands.hpp"
#include "x1nglsm/utils/arg_utils.hpp"
#include "x1nglsm/utils/glob_utils.hpp"
#include "x1nglsm/utils/string_utils.hpp"
#include "x1nglsm/utils/system_utils.hpp"
#include <climits>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace x1nglsm;

// ========== 帮助函数 ==========

namespace x1nglsm::cli {

// ========== 配置 ==========

std::string data_dir = "./data/cli"; // 默认数据目录

// ========== 打印函数 ==========

void print_help() {
  std::cout
      << "Available commands:\n"
      << "  SET/PUT <key> <value>   - Store a key-value pair\n"
      << "  GET <key>                - Retrieve value by key\n"
      << "  DEL <key>                - Delete a key\n"
      << "  MGET <key1> <key2> ...   - Retrieve multiple values\n"
      << "  MSET/MPUT <k1> <v1> ... - Store multiple key-value pairs\n"
      << "  MDEL <key1> <key2> ...   - Delete multiple keys\n"
      << "  KEYS [pattern]           - List keys matching pattern\n"
      << "  EXISTS <key>             - Check if key exists\n"
      << "  STRLEN <key>             - Get length of value\n"
      << "  APPEND <key> <value>     - Append value to key\n"
      << "  SETNX/PUTNX <key> <val>  - Set if key does not exist\n"
      << "  INCR <key>               - Increment integer by 1\n"
      << "  DECR <key>               - Decrement integer by 1\n"
      << "  INCRBY <key> <amount>    - Increment by amount\n"
      << "  DECRBY <key> <amount>    - Decrement by amount\n"
      << "  GETSET/GETPUT <k> <v>    - Get old value and set new value\n"
      << "  RENAME <key> <newkey>    - Rename a key\n"
      << "  PING                     - Check server connection\n"
      << "  INFO                     - Show database statistics\n"
      << "  FLUSHDB                  - Clear all data\n"
      << "  HELP                     - Show this help message\n"
      << "  EXIT                     - Exit the program\n"
      << "\n"
      << "Wildcard patterns for KEYS:\n"
      << "  *                        - Match any number of characters\n"
      << "  ?                        - Match exactly one character\n"
      << "\n"
      << "Examples:\n"
      << "  KEYS *                   - List all keys\n"
      << "  KEYS user:*              - List keys starting with 'user:'\n"
      << "  KEYS user:?              - List keys like 'user:a', 'user:1'\n"
      << "  KEYS *_test              - List keys ending with '_test'\n";
}

void print_args_help() {
  std::cout
      << "Usage: x1nglsm-cli [options]\n"
      << "Options:\n"
      << "  --dir <path>    Data directory (default: <project_root>/data/cli)\n"
      << "  --help, -h      Show this help message\n";
}

const std::unordered_map<std::string, CommandHandler> &get_commands() {
  static const std::unordered_map<std::string, CommandHandler> commands = {
      {"PUT", handle_put},
      {"SET", handle_put}, // PUT 的别名
      {"GET", handle_get},
      {"DEL", handle_delete},
      {"MGET", handle_mget},
      {"MPUT", handle_mput},
      {"MSET", handle_mput}, // MPUT 的别名
      {"MDEL", handle_mdel},
      {"KEYS", handle_keys},
      {"EXISTS", handle_exists},
      {"STRLEN", handle_strlen},
      {"APPEND", handle_append},
      {"SETNX", handle_setnx}, // PUTNX 的别名
      {"PUTNX", handle_setnx},
      {"INCR", handle_incr},
      {"DECR", handle_decr},
      {"INCRBY", handle_incrby},
      {"DECRBY", handle_decrby},
      {"GETSET", handle_getset}, // GETPUT 的别名
      {"GETPUT", handle_getset},
      {"RENAME", handle_rename},
      {"PING", handle_ping},
      {"INFO", handle_info},
      {"FLUSHDB", handle_flushdb},
      {"HELP", handle_help},
      {"EXIT", [](KVStore &) {
         std::cout << "Bye~" << std::endl;
         std::exit(0);
       }}};
  return commands;
}

// ========== 命令处理函数 ==========

/**
 * @brief 解析命令行参数（支持引号）
 * @param line 输入行
 * @return 参数列表
 */
std::vector<std::string> parse_args(const std::string &line) {
  std::vector<std::string> args;
  std::string current;
  bool in_quotes = false;

  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];

    if (c == '"') {
      in_quotes = !in_quotes;
    } else if (c == ' ' && !in_quotes) {
      if (!current.empty()) {
        args.emplace_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }

  // 添加最后一个参数
  if (!current.empty()) {
    args.emplace_back(current);
  }

  return args;
}

void handle_put(KVStore &store) {
  std::string line;

  // 检查是否还有输入
  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: PUT requires key and value\n";
    return;
  }

  // 读取整行剩余内容
  std::cin.ignore();
  std::getline(std::cin, line);

  // 解析参数（支持引号）
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: PUT requires key and value\n";
    return;
  }

  if (args.size() < 2) {
    std::cout << "Error: PUT requires value\n";
    return;
  }

  if (args.size() > 2) {
    std::cout << "Error: PUT accepts at most 2 arguments\n";
    return;
  }

  const std::string &key = args[0];
  const std::string &value = args[1];

  std::cout << (store.put(key, value) ? "OK" : "Error: PUT failed") << "\n";
}

void handle_get(KVStore &store) {
  std::string line;

  // 检查是否还有输入
  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: GET requires a key\n";
    return;
  }

  // 读取整行剩余内容
  std::cin.ignore();
  std::getline(std::cin, line);

  // 解析参数（支持引号）
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: GET requires a key\n";
    return;
  }

  if (args.size() > 1) {
    std::cout << "Error: GET accepts at most 1 argument\n";
    return;
  }

  const std::string &key = args[0];
  auto result = store.get(key);
  std::cout << (result.has_value() ? result.value() : "(nil)") << "\n";
}

void handle_delete(KVStore &store) {
  std::string line;

  // 检查是否还有输入
  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: DEL requires a key\n";
    return;
  }

  // 读取整行剩余内容
  std::cin.ignore();
  std::getline(std::cin, line);

  // 解析参数（支持引号）
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: DEL requires a key\n";
    return;
  }

  if (args.size() > 1) {
    std::cout << "Error: DEL accepts at most 1 argument\n";
    return;
  }

  const std::string &key = args[0];
  std::cout << (store.remove(key) ? "OK" : "Error: DEL failed") << "\n";
}

void handle_mget(KVStore &store) {
  std::string line;

  // 检查是否有参数
  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: MGET requires at least one key\n";
    return;
  }

  // 读取整行剩余内容
  std::cin.ignore();
  std::getline(std::cin, line);

  // 解析参数（支持引号）
  auto keys = parse_args(line);

  if (keys.empty()) {
    std::cout << "Error: MGET requires at least one key\n";
    return;
  }

  auto results = store.get(keys);
  for (size_t i = 0; i < keys.size(); ++i) {
    std::cout << i + 1 << ") " << keys[i] << ": "
              << (results[i].has_value() ? results[i].value() : "(nil)")
              << "\n";
  }
}

void handle_mput(KVStore &store) {
  std::string line;

  // 检查是否有参数
  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: MPUT requires at least one key-value pair\n";
    return;
  }

  // 读取整行剩余内容
  std::cin.ignore();
  std::getline(std::cin, line);

  // 解析参数（支持引号）
  auto args = parse_args(line);

  if (args.empty() || args.size() % 2 != 0) {
    std::cout << "Error: MPUT requires key-value pairs\n";
    return;
  }

  std::vector<std::pair<std::string, std::string>> kvs;
  for (size_t i = 0; i < args.size(); i += 2) {
    kvs.emplace_back(args[i], args[i + 1]);
  }

  std::cout << (store.put(kvs) ? "OK" : "Error: MPUT failed") << "\n";
}

void handle_mdel(KVStore &store) {
  std::string line;

  // 检查是否有参数
  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: MDEL requires at least one key\n";
    return;
  }

  // 读取整行剩余内容
  std::cin.ignore();
  std::getline(std::cin, line);

  // 解析参数（支持引号）
  auto keys = parse_args(line);

  if (keys.empty()) {
    std::cout << "Error: MDEL requires at least one key\n";
    return;
  }

  std::cout << (store.remove(keys) ? "OK" : "Error: MDEL failed") << "\n";
}

void handle_keys(KVStore &store) {
  std::string line;
  std::string pattern = "*"; // 默认匹配所有 key

  // 检查是否有参数（模式）
  if (std::cin.peek() != '\n') {
    std::cin.ignore();
    std::getline(std::cin, line);
    auto args = parse_args(line);

    if (args.size() > 1) {
      std::cout << "Error: KEYS accepts at most one argument (pattern)\n";
      return;
    }

    if (!args.empty()) {
      pattern = args[0];
    }
  } else {
    std::cin.ignore();
  }

  auto all_keys = store.keys();

  // 使用通配符过滤
  std::vector<std::string> filtered_keys;
  for (const auto &key : all_keys) {
    if (utils::glob_match(pattern, key)) {
      filtered_keys.emplace_back(key);
    }
  }

  if (filtered_keys.empty()) {
    std::cout << "(empty set)\n";
    return;
  }

  for (size_t i = 0; i < filtered_keys.size(); ++i) {
    std::cout << i + 1 << ") " << filtered_keys[i] << "\n";
  }
}

void handle_exists(KVStore &store) {
  std::string line;

  // 检查是否还有输入
  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: EXISTS requires a key\n";
    return;
  }

  // 读取整行剩余内容
  std::cin.ignore();
  std::getline(std::cin, line);

  // 解析参数（支持引号）
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: EXISTS requires a key\n";
    return;
  }

  if (args.size() > 1) {
    std::cout << "Error: EXISTS accepts at most 1 argument\n";
    return;
  }

  const std::string &key = args[0];
  std::cout << (store.exists(key) ? "1" : "0") << "\n";
}

void handle_info(KVStore &store) {
  std::string line;

  if (std::cin.peek() != '\n') {
    std::cin.ignore();
    std::getline(std::cin, line);
    auto args = parse_args(line);

    if (!args.empty()) {
      std::cout << "Error: INFO accepts no arguments\n";
      return;
    }
  } else {
    std::cin.ignore();
  }

  auto abs_path = std::filesystem::absolute(data_dir).lexically_normal();

  // 计算总磁盘使用
  size_t disk_usage = 0;
  if (std::filesystem::exists(abs_path)) {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(abs_path)) {
      if (entry.is_regular_file()) {
        disk_usage += entry.file_size();
      }
    }
  }

  // 使用format_size工具函数
  std::cout << "# Statistics\n"
            << "  keys:       " << store.size() << "\n"
            << "  sstables:   " << store.sstables_count() << "\n"
            << "  disk_usage: " << utils::format_size(disk_usage) << "\n"
            << "  mem_table:  " << utils::format_size(store.mem_usage())
            << " (internal)\n"
            << "  wal:        " << utils::format_size(store.wal_size()) << "\n"
            << "  data_dir:   " << abs_path << "\n";
}

void handle_help(KVStore &store) {
  (void)store; // 未使用参数

  std::string line;

  if (std::cin.peek() != '\n') {
    std::cin.ignore();
    std::getline(std::cin, line);
    auto args = parse_args(line);

    if (!args.empty()) {
      std::cout << "Error: HELP accepts no arguments\n";
      return;
    }
  } else {
    std::cin.ignore();
  }

  print_help();
}

void handle_flushdb(KVStore &store) {
  std::cout << "Are you sure? This will delete all data. (yes/no): ";
  std::string confirm;
  std::cin >> confirm;
  // 清空整行剩余输入（如果有）
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (utils::to_upper(confirm) == "YES") {
    store.clear();
    std::cout << "OK\n";
  } else {
    std::cout << "Aborted\n";
  }
}

void handle_strlen(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: STRLEN requires a key\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: STRLEN requires a key\n";
    return;
  }

  if (args.size() > 1) {
    std::cout << "Error: STRLEN accepts at most 1 argument\n";
    return;
  }

  const std::string &key = args[0];
  auto result = store.get(key);
  if (result.has_value()) {
    std::cout << result.value().size() << "\n";
  } else {
    std::cout << "0\n";
  }
}

void handle_append(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: APPEND requires key and value\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: APPEND requires key and value\n";
    return;
  }

  if (args.size() < 2) {
    std::cout << "Error: APPEND requires value\n";
    return;
  }

  if (args.size() > 2) {
    std::cout << "Error: APPEND accepts at most 2 arguments\n";
    return;
  }

  const std::string &key = args[0];
  const std::string &value = args[1];

  auto old_value = store.get(key);
  std::string new_value;
  if (old_value.has_value()) {
    new_value = old_value.value() + value;
  } else {
    new_value = value;
  }

  if (store.put(key, new_value)) {
    std::cout << new_value.size() << "\n";
  } else {
    std::cout << "Error: APPEND failed\n";
  }
}

void handle_setnx(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: SETNX requires key and value\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: SETNX requires key and value\n";
    return;
  }

  if (args.size() < 2) {
    std::cout << "Error: SETNX requires value\n";
    return;
  }

  if (args.size() > 2) {
    std::cout << "Error: SETNX accepts at most 2 arguments\n";
    return;
  }

  const std::string &key = args[0];
  const std::string &value = args[1];

  if (store.exists(key)) {
    std::cout << "0\n";
  } else {
    std::cout << (store.put(key, value) ? "1" : "0") << "\n";
  }
}

void handle_incr(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: INCR requires a key\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: INCR requires a key\n";
    return;
  }

  if (args.size() > 1) {
    std::cout << "Error: INCR accepts at most 1 argument\n";
    return;
  }

  const std::string &key = args[0];
  auto result = store.get(key);

  if (!result.has_value()) {
    // Redis 行为：不存在的 key 初始化为 0 再操作
    store.put(key, "0");
    result = "0";
  }

  try {
    long old_val = std::stol(result.value());
    if (old_val > LONG_MAX - 1) {
      std::cout << "Error: INCR overflow\n";
      return;
    }
    long new_val = old_val + 1;
    if (store.put(key, std::to_string(new_val))) {
      std::cout << new_val << "\n";
    } else {
      std::cout << "Error: INCR failed\n";
    }
  } catch (const std::exception &) {
    std::cout << "Error: INCR on non-integer value\n";
  }
}

void handle_decr(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: DECR requires a key\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: DECR requires a key\n";
    return;
  }

  if (args.size() > 1) {
    std::cout << "Error: DECR accepts at most 1 argument\n";
    return;
  }

  const std::string &key = args[0];
  auto result = store.get(key);

  if (!result.has_value()) {
    // Redis 行为：不存在的 key 初始化为 0 再操作
    store.put(key, "0");
    result = "0";
  }

  try {
    long old_val = std::stol(result.value());
    if (old_val < LONG_MIN + 1) {
      std::cout << "Error: DECR underflow\n";
      return;
    }
    long new_val = old_val - 1;
    if (store.put(key, std::to_string(new_val))) {
      std::cout << new_val << "\n";
    } else {
      std::cout << "Error: DECR failed\n";
    }
  } catch (const std::exception &) {
    std::cout << "Error: DECR on non-integer value\n";
  }
}

void handle_incrby(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: INCRBY requires key and increment\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: INCRBY requires key and increment\n";
    return;
  }

  if (args.size() < 2) {
    std::cout << "Error: INCRBY requires increment\n";
    return;
  }

  if (args.size() > 2) {
    std::cout << "Error: INCRBY accepts at most 2 arguments\n";
    return;
  }

  const std::string &key = args[0];
  const std::string &increment_str = args[1];

  auto result = store.get(key);

  if (!result.has_value()) {
    // Redis 行为：不存在的 key 初始化为 0 再操作
    store.put(key, "0");
    result = "0";
  }

  try {
    long old_val = std::stol(result.value());
    long increment = std::stol(increment_str);
    // 检查溢出：increment > 0 时检查上溢，increment < 0 时检查下溢
    if (increment > 0 && old_val > LONG_MAX - increment) {
      std::cout << "Error: INCRBY overflow\n";
      return;
    }
    if (increment < 0 && old_val < LONG_MIN - increment) {
      std::cout << "Error: INCRBY underflow\n";
      return;
    }
    long new_val = old_val + increment;
    if (store.put(key, std::to_string(new_val))) {
      std::cout << new_val << "\n";
    } else {
      std::cout << "Error: INCRBY failed\n";
    }
  } catch (const std::exception &) {
    std::cout << "Error: INCRBY on non-integer value\n";
  }
}

void handle_decrby(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: DECRBY requires key and decrement\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: DECRBY requires key and decrement\n";
    return;
  }

  if (args.size() < 2) {
    std::cout << "Error: DECRBY requires decrement\n";
    return;
  }

  if (args.size() > 2) {
    std::cout << "Error: DECRBY accepts at most 2 arguments\n";
    return;
  }

  const std::string &key = args[0];
  const std::string &decrement_str = args[1];

  auto result = store.get(key);

  if (!result.has_value()) {
    // Redis 行为：不存在的 key 初始化为 0 再操作
    store.put(key, "0");
    result = "0";
  }

  try {
    long old_val = std::stol(result.value());
    long decrement = std::stol(decrement_str);
    // 检查溢出：decrement > 0 时检查下溢，decrement < 0 时检查上溢
    if (decrement > 0 && old_val < LONG_MIN + decrement) {
      std::cout << "Error: DECRBY underflow\n";
      return;
    }
    if (decrement < 0 && old_val > LONG_MAX + decrement) {
      std::cout << "Error: DECRBY overflow\n";
      return;
    }
    long new_val = old_val - decrement;
    if (store.put(key, std::to_string(new_val))) {
      std::cout << new_val << "\n";
    } else {
      std::cout << "Error: DECRBY failed\n";
    }
  } catch (const std::exception &) {
    std::cout << "Error: DECRBY on non-integer value\n";
  }
}

void handle_getset(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: GETSET requires key and value\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: GETSET requires key and value\n";
    return;
  }

  if (args.size() < 2) {
    std::cout << "Error: GETSET requires value\n";
    return;
  }

  if (args.size() > 2) {
    std::cout << "Error: GETSET accepts at most 2 arguments\n";
    return;
  }

  const std::string &key = args[0];
  const std::string &value = args[1];

  auto old_value = store.get(key);
  if (store.put(key, value)) {
    std::cout << (old_value.has_value() ? old_value.value() : "(nil)") << "\n";
  } else {
    std::cout << "Error: GETSET failed\n";
  }
}

void handle_rename(KVStore &store) {
  std::string line;

  if (std::cin.peek() == '\n') {
    std::cin.ignore();
    std::cout << "Error: RENAME requires key and newkey\n";
    return;
  }

  std::cin.ignore();
  std::getline(std::cin, line);
  auto args = parse_args(line);

  if (args.empty()) {
    std::cout << "Error: RENAME requires key and newkey\n";
    return;
  }

  if (args.size() < 2) {
    std::cout << "Error: RENAME requires newkey\n";
    return;
  }

  if (args.size() > 2) {
    std::cout << "Error: RENAME accepts at most 2 arguments\n";
    return;
  }

  const std::string &key = args[0];
  const std::string &newkey = args[1];

  // 获取旧值（同时确认源 key 存在）
  auto value = store.get(key);
  if (!value.has_value()) {
    std::cout << "Error: RENAME source key does not exist\n";
    return;
  }

  // 先写新 key（成功后再删旧 key，崩溃时最多新旧同时存在，不会丢数据）
  if (!store.put(newkey, value.value())) {
    std::cout << "Error: RENAME failed\n";
    return;
  }

  // 再删旧 key
  store.remove(key);
  std::cout << "OK\n";
}

void handle_ping(KVStore &store) {
  (void)store; // Unused

  std::string line;

  // 检查是否有参数
  if (std::cin.peek() != '\n') {
    std::cin.ignore();
    std::getline(std::cin, line);
    auto args = parse_args(line);

    if (!args.empty()) {
      std::cout << "Error: PING accepts no arguments\n";
      return;
    }
  } else {
    std::cin.ignore();
  }

  std::cout << "PONG\n";
}

void initialize() {
  // 设置控制台为UTF-8编码（跨平台）
  utils::set_utf8_encoding();

  // 初始化数据目录（处理相对路径，自动定位项目根目录）
  utils::initialize_data_directory(data_dir);
}

void run_command_loop(KVStore &store) {
  const auto &commands = get_commands();
  std::string cmd;

  while (std::cout << "> " && std::cin >> cmd) {
    cmd = utils::to_upper(cmd);

    auto it = commands.find(cmd);
    if (it != commands.end()) {
      it->second(store);
    } else {
      // 清空整行剩余输入，避免被当作后续命令
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "Unknown command. Type HELP for usage.\n";
    }
  }
}

int run_cli(int argc, char *argv[]) {
  // 初始化CLI环境
  initialize();

  // 解析命令行参数
  utils::parse_args(argc, argv, data_dir);

  // 创建KVStore实例
  KVStore store(data_dir);

  // 运行命令循环
  run_command_loop(store);

  return 0;
}

} // namespace x1nglsm::cli