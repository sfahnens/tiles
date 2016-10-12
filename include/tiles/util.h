#pragma once

#include <map>

#include "geo/webmercator.h"

namespace tiles {

inline rocksdb::spatial::BoundingBox<double> bbox(geo::bounds const& b) {
  return {b.minx_, b.miny_, b.maxx_, b.maxy_};
}

inline rocksdb::spatial::BoundingBox<double> bbox(geo::xy const& m) {
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

}  // namespace tiles
