#include "x1nglsm/core/entry.hpp"

#include <cstring>
#include <utility>

namespace x1nglsm::core {

Entry::Entry(std::string key, std::string value, OpType type,
             uint64_t timestamp)
    : key(std::move(key)), value(std::move(value)), type(type),
      timestamp(timestamp) {}

std::string Entry::encode() const {
  // [type(1)][timestamp(8)][klen(4)][key][vlen(4)][value]
  std::string result;
  result.reserve(encode_size());

  // 1.写入 type（1字节）
  result.push_back(static_cast<char>(type));

  // 2.写入 timestamp（8字节，小端序）
  char ts_buf[8];
  std::memcpy(ts_buf, &timestamp, 8);
  result.append(ts_buf, 8);

  // 3.写入 key长度（4字节）
  uint32_t klen = key.size();
  char klen_buf[4];
  std::memcpy(klen_buf, &klen, 4);
  result.append(klen_buf, 4);

  // 4.写入 key 内容
  result.append(key);

  // 5.写入 value长度（4字节）
  uint32_t vlen = value.size();
  char vlen_buf[4];
  std::memcpy(vlen_buf, &vlen, 4);
  result.append(vlen_buf, 4);

  // 6.写入 value内容
  result.append(value);

  return result;
}

std::optional<Entry> Entry::decode(const std::string &data) {
  // [type(1)][timestamp(8)][klen(4)][key][vlen(4)][value]
  Entry entry;
  size_t pos = 0;
  size_t n = data.size();

  // 1.读取 type（1字节）
  if (pos + 1 > n)
    return std::nullopt;
  entry.type = static_cast<OpType>(data[pos]);
  pos += 1;

  // 2. 读取 timestamp（8字节）
  if (pos + 8 > n)
    return std::nullopt;
  std::memcpy(&entry.timestamp, data.data() + pos, 8);
  pos += 8;

  // 3. 读取 key长度（4字节）
  if (pos + 4 > n)
    return std::nullopt;
  uint32_t klen;
  std::memcpy(&klen, data.data() + pos, 4);
  pos += 4;

  // 4. 读取 key内容
  if (pos + klen > n)
    return std::nullopt;
  entry.key = data.substr(pos, klen);
  pos += klen;

  // 5. 读取 value长度（4字节）
  if (pos + 4 > n)
    return std::nullopt;
  uint32_t vlen;
  std::memcpy(&vlen, data.data() + pos, 4);
  pos += 4;

  // 6. 读取 value内容
  if (pos + vlen > n)
    return std::nullopt;
  entry.value = data.substr(pos, vlen);

  return entry;
}

size_t Entry::encode_size() const {
  // 固定部分：type(1) + timestamp(8) + klen(4) + vlen(4) = 17
  return 17 + key.size() + value.size();
}

} // namespace x1nglsm::core