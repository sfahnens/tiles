#pragma once

#include <cstring>

namespace tiles {

template <typename T>
void append(std::string& buf, T const& t) {
  buf.append(reinterpret_cast<char const*>(&t), sizeof(T));
}

template <typename T>
void write(char* data, size_t offset, T const& t) {
  std::memcpy(data + offset, reinterpret_cast<char const*>(&t), sizeof(T));
}

template <typename T>
void write_nth(char* data, size_t offset, T const& t) {
  return write<T>(data, offset * sizeof(t), t);
}

template <typename T>
T read(char const* data, size_t offset = 0UL) {
  T t;
  std::memcpy(&t, data + offset, sizeof(T));
  return t;
}

template <typename T>
T read_nth(char const* data, size_t offset) {
  return read<T>(data, offset * sizeof(T));
}

inline bool bit_set(uint32_t val, uint32_t idx) {
  return (val & (1 << idx)) != 0;
}

}  // namespace tiles