#pragma once

#include <cstdint>
#include <string>
#include <tuple>

#include "tiles/bin_utils.h"

namespace tiles {

enum class metadata_value_t : uint8_t {
  bool_false = 0,
  bool_true = 1,
  string = 2,
  numeric = 3,
  integer = 4
};

inline std::string encode_bool(bool v) {
  std::string buf;
  append(buf, v ? metadata_value_t::bool_true : metadata_value_t::bool_false);
  return buf;
}

inline std::string encode_string(std::string const& v) {
  std::string buf;
  append(buf, metadata_value_t::string);
  buf.append(v);
  return buf;
}

inline std::string encode_numeric(double v) {
  std::string buf;
  append(buf, metadata_value_t::numeric);
  append(buf, v);
  return buf;
}

inline std::string encode_integer(int64_t v) {
  std::string buf;
  append(buf, metadata_value_t::integer);
  append(buf, v);
  return buf;
}

struct metadata {
  metadata() = default;
  metadata(std::string key, std::string value)
      : key_{std::move(key)}, value_{std::move(value)} {}

  friend bool operator==(metadata const& lhs, metadata const& rhs) {
    return std::tie(lhs.key_, lhs.value_) == std::tie(rhs.key_, rhs.value_);
  }

  friend bool operator!=(metadata const& lhs, metadata const& rhs) {
    return std::tie(lhs.key_, lhs.value_) != std::tie(rhs.key_, rhs.value_);
  }

  friend bool operator<(metadata const& lhs, metadata const& rhs) {
    return std::tie(lhs.key_, lhs.value_) < std::tie(rhs.key_, rhs.value_);
  }

  std::string key_, value_;
};

}  // namespace tiles
