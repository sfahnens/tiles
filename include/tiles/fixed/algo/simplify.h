#pragma once

#include "geo/simplify_mask.h"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

inline void simplify(fixed_null&, uint32_t const) {}

inline fixed_geometry simplify(fixed_point multi_point, uint32_t const) {
  return std::move(multi_point);
}

inline fixed_geometry simplify(fixed_polyline multi_polyline,
                               uint32_t const z) {
  for (auto& polyline : multi_polyline) {
    geo::simplify(polyline, z);
  }
  return std::move(multi_polyline);
}

inline fixed_geometry simplify(fixed_polygon multi_polygon, uint32_t const) {
  return std::move(multi_polygon);
}

inline fixed_geometry simplify(fixed_geometry geometry, uint32_t const z) {
  return mpark::visit([&](auto&& arg) { return simplify(std::move(arg), z); },
                      std::move(geometry));
}

}  // namespace tiles
