# 示例程序

构建后位于 `bin/examples/` 目录。

### example_basic — 基础操作

展示 PUT、GET、DELETE、EXISTS、KEYS 等基本 CRUD 操作。

```bash
./bin/examples/example_basic
```

### example_batch — 批量操作

展示批量写入 1000 个 key-value、批量查询、批量删除。

```bash
./bin/examples/example_batch
```

### example_persistence — 持久化和恢复

展示数据写入磁盘后，重新打开 KVStore 自动恢复。运行后选择模式 1 写入，再运行选择模式 2 验证恢复。

```bash
./bin/examples/example_persistence
```

### example_wildcard — 通配符查询

展示使用 `glob_match` 进行模式匹配查询，支持 `*` 和 `?` 通配符。

```bash
./bin/examples/example_wildcard
```
