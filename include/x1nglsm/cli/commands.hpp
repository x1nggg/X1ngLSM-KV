#pragma once

#include "x1nglsm/kv_store.hpp"

#include <string>
#include <unordered_map>

namespace x1nglsm::cli {

// 外部变量：数据目录
extern std::string data_dir;

// ========== 类型定义 ==========

using CommandHandler = void (*)(KVStore &);

// ========== 帮助函数 ==========

void print_help();

void print_args_help();

// ========== 命令处理函数 ==========

void handle_put(KVStore &store);

void handle_get(KVStore &store);

void handle_delete(KVStore &store);

void handle_mget(KVStore &store);

void handle_mput(KVStore &store);

void handle_mdel(KVStore &store);

void handle_keys(KVStore &store);

void handle_exists(KVStore &store);

void handle_info(KVStore &store);

void handle_help(KVStore &store);

void handle_flushdb(KVStore &store);

void handle_strlen(KVStore &store);

void handle_append(KVStore &store);

void handle_setnx(KVStore &store);

void handle_incr(KVStore &store);

void handle_decr(KVStore &store);

void handle_incrby(KVStore &store);

void handle_decrby(KVStore &store);

void handle_getset(KVStore &store);

void handle_rename(KVStore &store);

void handle_ping(KVStore &store);

// ========== 命令注册 ==========

/**
 * @brief 获取命令映射表
 * @return 命令字符串到处理函数的映射表
 */
const std::unordered_map<std::string, CommandHandler> &get_commands();

/**
 * @brief 运行命令循环
 * @param store KVStore实例
 */
void run_command_loop(KVStore &store);

/**
 * @brief 运行CLI程序
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 程序退出码
 */
int run_cli(int argc, char *argv[]);

} // namespace x1nglsm::cli