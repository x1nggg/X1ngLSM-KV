#include <utility>

#include "x1nglsm/core/write_ahead_log.hpp"

namespace x1nglsm::core {

WriteAheadLog::WriteAheadLog(std::string file_path)
    : file_path_(std::move(file_path)), current_size_(0) {
  file_.open(file_path_, std::ios::binary | std::ios::app);

  // 获取当前文件大小
  if (file_.is_open()) {
    file_.seekp(0, std::ios::end);
    current_size_ = file_.tellp();
  }
}

bool WriteAheadLog::append(const Entry &entry) {
  // 单Entry：立即刷盘
  return do_append(entry) && sync();
}

bool WriteAheadLog::append(const std::vector<Entry> &entries) {
  // 批量Entry：延迟刷盘
  for (const auto &entry : entries) {
    if (!do_append(entry)) {
      return false;
    }
  }
  return sync();
}

std::vector<Entry> WriteAheadLog::read_all() {
  std::vector<Entry> entries;

  std::ifstream file(file_path_, std::ios::binary);
  if (!file.is_open())
    return entries;

  // 循环读取
  while (file) {
    // 读取4字节长度
    uint32_t len;
    file.read(reinterpret_cast<char *>(&len), sizeof(len));

    if (!file || file.eof())
      break;

    // 读取Entry序列化数据
    std::string data(len, '\0');
    file.read(&data[0], len);

    if (!file)
      break;

    // 反序列化为Entry
    auto entry_opt = Entry::decode(data);
    if (entry_opt.has_value())
      entries.emplace_back(entry_opt.value());
  }

  return entries;
}

bool WriteAheadLog::do_append(const Entry &entry) {
  if (!file_.is_open()) {
    return false;
  }

  // 序列化Entry
  const std::string &data = entry.encode();

  // 写入长度前缀（4字节）
  auto len = static_cast<uint32_t>(data.size());
  file_.write(reinterpret_cast<const char *>(&len), sizeof(len));

  // 写入Entry数据
  file_.write(data.data(), data.size());

  // 检查写入是否成功
  if (!file_.good()) {
    return false;
  }

  // 更新文件大小
  current_size_ += sizeof(len) + data.size();

  return true;
}

bool WriteAheadLog::sync() {
  if (file_.is_open()) {
    file_.flush();
    return file_.good();
  }
  return false;
}

void WriteAheadLog::clear() {
  // 关闭当前文件
  close();

  // 清空文件
  std::ofstream(file_path_, std::ios::binary | std::ios::trunc).close();

  // 重新打开
  file_.open(file_path_, std::ios::binary | std::ios::app);

  current_size_ = 0;
}

void WriteAheadLog::close() {
  if (file_.is_open())
    file_.close();
}

} // namespace x1nglsm::core