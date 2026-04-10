#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace x1nglsm::utils {

/**
 * @brief CRC32 查找表（编译期生成）
 * @details 对 0~255 每个值，模拟处理 8 个 bit 的 CRC 运算，
 *          生成查找表。运行时通过查表实现快速 CRC32 计算。
 *
 * 多项式: 0xEDB88320（CRC32 标准反向表示）
 */
struct CRC32Table {
  uint32_t data[256];

  constexpr CRC32Table() : data{} {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t crc = i;
      // 模拟处理一个字节的 8 个 bit
      for (int j = 0; j < 8; ++j) {
        // 最低位为 1 则右移后异或多项式，否则直接右移
        crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
      }
      data[i] = crc;
    }
  }
};

// 全局查找表实例，inline 保证头文件多重包含安全
inline constexpr CRC32Table kCRC32Table{};

/**
 * @brief 计算 CRC32 校验和
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC32 校验值
 */
inline uint32_t crc32(const void *data, size_t len) {
  const auto *bytes = static_cast<const uint8_t *>(data);
  uint32_t crc = 0xFFFFFFFF; // 初始值全 1，保证前导零参与计算
  for (size_t i = 0; i < len; ++i) {
    // 查表 + 异或：当前 CRC 最低字节与输入字节异或后查表，再与 CRC 右移 8 位异或
    crc = kCRC32Table.data[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
  }

  return crc ^ 0xFFFFFFFF; // 最终取反（CRC32 标准要求）
}

inline uint32_t crc32(const std::string &data) {
  return crc32(data.data(), data.size());
}

} // namespace x1nglsm::utils
