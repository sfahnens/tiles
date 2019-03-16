#pragma once

#include "boost/geometry.hpp"

namespace tiles {

inline fixed_coord_t area(fixed_null const&) { return 0; }
inline fixed_coord_t area(fixed_xy const&) { return 0; }
inline fixed_coord_t area(fixed_polyline const&) { return 0; }

inline fixed_coord_t area(fixed_polygon const& multi_polygon) {
  return boost::geometry::area(multi_polygon);
}

inline fixed_coord_t area(fixed_box const& box) {
  return boost::geometry::area(box);
}

inline fixed_coord_t area(fixed_simple_polygon const& simple_polygon) {
  return boost::geometry::area(simple_polygon);
}

inline fixed_coord_t area(fixed_geometry const& geometry) {
  return mpark::visit([&](auto const& arg) { return area(arg); }, geometry);
}

}  // namespace tiles
