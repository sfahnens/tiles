#pragma once

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

inline void shift(fixed_null&, uint32_t const) {}

inline void shift(fixed_xy& pt, uint32_t const z) {
  uint32_t delta_z = 20 - z;
  pt.x(pt.x() >> delta_z);
  pt.y(pt.y() >> delta_z);
}

template <typename Container>
inline void shift(Container& c, uint32_t const z) {
  for (auto& e : c) {
    shift(e, z);
  }
}

inline void shift(fixed_polyline& multi_polyline, uint32_t const z) {
  for (auto& polyline : multi_polyline) {
    shift(polyline, z);
  }
}

inline void shift(fixed_polygon& multi_polygon, uint32_t const z) {
  for (auto& polygon : multi_polygon) {
    shift(polygon.outer(), z);
    for (auto& ring : polygon.inners()) {
      shift(ring, z);
    }
  }
}

inline void shift(fixed_geometry& geometry, uint32_t const z) {
  mpark::visit([&](auto& arg) { shift(arg, z); }, geometry);
}

inline void shift(fixed_box& box, uint32_t const z) {
  shift(box.min_corner(), z);
  shift(box.max_corner(), z);
}

}  // namespace tiles
