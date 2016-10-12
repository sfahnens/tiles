#pragma once

#include <vector>

#include "rocksdb/slice.h"

namespace tiles {

// TODO make this zerocopy!?
template <typename T>
std::vector<T> from_slice(rocksdb::Slice const& slice) {
  T const* base = reinterpret_cast<T const*>(slice.data());
  T const* end = base + (slice.size() / sizeof(T));
  return {base, end};
}

template <typename T>
rocksdb::Slice to_slice(std::vector<T> const& vec) {
  return {reinterpret_cast<char const*>(vec.data()), vec.size() * sizeof(T)};
}

}  // namespace tiles