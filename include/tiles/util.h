#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <map>
#include <stdexcept>

#include "geo/webmercator.h"

#include "rocksdb/utilities/spatial_db.h"

namespace tiles {

inline rocksdb::spatial::BoundingBox<double> bbox(geo::merc_bounds const& b) {
  return {b.minx_, b.miny_, b.maxx_, b.maxy_};
}

inline rocksdb::spatial::BoundingBox<double> bbox(geo::pixel_bounds const& b) {
  return {static_cast<double>(b.minx_), static_cast<double>(b.miny_),
          static_cast<double>(b.maxx_), static_cast<double>(b.maxy_)};
}

inline rocksdb::spatial::BoundingBox<double> bbox(geo::merc_xy const& m) {
  return {m.x_, m.y_, m.x_, m.y_};
}

// XXX we need some utils lib!!

template <typename K, typename V, typename CreateFun>
V& get_or_create(std::map<K, V>& m, K const& key, CreateFun f) {
  auto it = m.find(key);
  if (it != end(m)) {
    return it->second;
  } else {
    return m[key] = f();
  }
}

template <typename K, typename Less>
size_t get_or_create_index(std::map<K, size_t, Less>& m, K const& key) {
  auto it = m.find(key);
  if (it != end(m)) {
    return it->second;
  } else {
    return m[key] = m.size();
  }
}

template <typename It, typename UnaryOperation>
inline auto transform_to_vec(It s, It e, UnaryOperation op)
    -> std::vector<decltype(op(*s))> {
  std::vector<decltype(op(*s))> vec(std::distance(s, e));
  std::transform(s, e, std::begin(vec), op);
  return vec;
}

template <typename Container, typename UnaryOperation>
inline auto transform_to_vec(Container const& c, UnaryOperation op)
    -> std::vector<decltype(op(*std::begin(c)))> {
  std::vector<decltype(op(*std::begin(c)))> vec(
      std::distance(std::begin(c), std::end(c)));
  std::transform(std::begin(c), std::end(c), std::begin(vec), op);
  return vec;
}

}  // namespace tiles

#ifndef log_err
#define log_err(M, ...) fprintf(stderr, "[ERR] " M "\n", ##__VA_ARGS__);
#endif

#ifdef verify
#undef verify
#endif

#define verify(A, M, ...)        \
  if (!(A)) {                    \
    log_err(M, ##__VA_ARGS__);   \
    throw std::runtime_error(M); \
  }
