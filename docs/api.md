# API 参考

## KVStore

主接口，命名空间 `x1nglsm`，头文件 `<x1nglsm/kv_store.hpp>`。

### 构造与析构

```cpp
// 打开或创建数据库，传入数据目录路径和可选的 flush 阈值（默认 32MB），自动从 SSTable 和 WAL 恢复
explicit KVStore(std::string db_dir, size_t flush_threshold = 32 * 1024 * 1024);

// 不可拷贝
KVStore(const KVStore&) = delete;
KVStore& operator=(const KVStore&) = delete;

~KVStore() = default;
```

### 写入

```cpp
// 写入单个 key-value，返回是否成功
bool put(const std::string &key, const std::string &value);

// 批量写入，返回是否成功（全部写入或全部失败）
bool put(const std::vector<std::pair<std::string, std::string>> &kvs);
```

### 查询

```cpp
// 查询单个 key，不存在返回 std::nullopt
std::optional<std::string> get(const std::string &key) const;

// 批量查询，返回与 keys 等长的 vector，每个元素为 optional
std::vector<std::optional<std::string>>
get(const std::vector<std::string> &keys) const;
```

### 删除

```cpp
// 删除单个 key（写入墓碑），返回是否成功
bool remove(const std::string &key);

// 批量删除
bool remove(const std::vector<std::string> &keys);
```

### 状态查询

```cpp
// 检查 key 是否存在
bool exists(const std::string &key) const;

// 获取所有有效 key
std::vector<std::string> keys() const;

// key 总数
size_t size() const;

// SSTable 文件数量
size_t sstables_count() const;

// MemTable 内存使用（字节），包含活跃 MemTable 和 Immutable MemTable
size_t mem_usage() const;

// WAL 文件大小（字节）
size_t wal_size() const;

// 清空所有数据（MemTable、WAL、SSTable）
void clear();
```

### 使用示例

```cpp
#include <x1nglsm/kv_store.hpp>
#include <iostream>

int main() {
    x1nglsm::KVStore store("./data");

    // 写入
    store.put("name", "x1ng");
    store.put({{"a", "1"}, {"b", "2"}, {"c", "3"}});

    // 查询
    auto val = store.get("name");
    if (val) std::cout << *val << "\n";  // x1ng

    auto vals = store.get({"a", "b", "z"});
    // vals = {optional("1"), optional("2"), nullopt}

    // 删除
    store.remove("a");

    // 状态
    std::cout << store.size() << "\n";           // key 数量
    std::cout << store.mem_usage() << "\n";       // 内存使用
    std::cout << store.sstables_count() << "\n";  // SSTable 数量

    // 清空
    store.clear();
    return 0;
}
```

---

## CLI

命令行工具，头文件 `<x1nglsm/cli/commands.hpp>`。

### 启动

```bash
./bin/cli/x1nglsm-cli              # 默认数据目录 ./data/cli
./bin/cli/x1nglsm-cli --dir /path/to/db  # 指定数据目录
```

### 命令列表

#### 基本操作

| 命令     | 格式                | 说明                 |
| -------- | ------------------- | -------------------- |
| `put`    | `put <key> <value>` | 写入                 |
| `get`    | `get <key>`         | 查询                 |
| `del`    | `del <key>`         | 删除                 |
| `mput`   | `mput`              | 批量写入（交互输入） |
| `mget`   | `mget`              | 批量查询（交互输入） |
| `mdel`   | `mdel`              | 批量删除（交互输入） |
| `keys`   | `keys`              | 列出所有 key         |
| `exists` | `exists <key>`      | 检查 key 是否存在    |
| `info`   | `info`              | 数据库状态信息       |

#### 字符串操作

| 命令     | 格式                   | 说明             |
| -------- | ---------------------- | ---------------- |
| `strlen` | `strlen <key>`         | 获取 value 长度  |
| `append` | `append <key> <value>` | 追加值到已有 key |

#### 数值操作

| 命令     | 格式               | 说明                                        |
| -------- | ------------------ | ------------------------------------------- |
| `incr`   | `incr <key>`       | 自增 1（key 不存在时初始化为 0 再操作）     |
| `decr`   | `decr <key>`       | 自减 1（key 不存在时初始化为 0 再操作）     |
| `incrby` | `incrby <key> <n>` | 增加指定值（key 不存在时初始化为 0 再操作） |
| `decrby` | `decrby <key> <n>` | 减少指定值（key 不存在时初始化为 0 再操作） |

#### 其他

| 命令      | 格式                    | 说明                  |
| --------- | ----------------------- | --------------------- |
| `setnx`   | `setnx <key> <value>`   | 仅当 key 不存在时写入 |
| `getset`  | `getset <key> <value>`  | 设置新值并返回旧值    |
| `rename`  | `rename <key> <newkey>` | 重命名 key            |
| `ping`    | `ping`                  | 测试连接              |
| `flushdb` | `flushdb`               | 清空数据库            |
| `help`    | `help`                  | 帮助信息              |

---

## 工具函数

### glob_utils

头文件 `<x1nglsm/utils/glob_utils.hpp>`，命名空间 `x1nglsm::utils`。

```cpp
// Glob 模式匹配，支持 * (任意字符) 和 ? (单字符)
bool glob_match(const std::string &pattern, const std::string &text);

// 示例
glob_match("user:*", "user:123");  // true
glob_match("user:?", "user:a");    // true
glob_match("user:?", "user:ab");   // false
```

### string_utils

头文件 `<x1nglsm/utils/string_utils.hpp>`。

```cpp
// 转大写
std::string to_upper(std::string s);

// 格式化字节数为可读字符串
std::string format_size(size_t bytes);  // 1536 → "1.50 KB"
```

### system_utils

头文件 `<x1nglsm/utils/system_utils.hpp>`。

```cpp
// 获取当前可执行文件路径
std::filesystem::path get_executable_path();

// 向上查找项目根目录（定位 CMakeLists.txt）
std::filesystem::path find_project_root();

// 设置控制台 UTF-8 编码
void set_utf8_encoding();

// 初始化数据目录（处理相对路径）
void initialize_data_directory(std::string &data_dir);
```

### arg_utils

头文件 `<x1nglsm/utils/arg_utils.hpp>`。

```cpp
// 解析命令行参数，输出数据目录路径
void parse_args(int argc, char *argv[], std::string &out_dir);
```

### crc32

头文件 `<x1nglsm/utils/crc32.hpp>`。

```cpp
// 计算 CRC32 校验和
uint32_t crc32(const void *data, size_t len);
uint32_t crc32(const std::string &data);
```
