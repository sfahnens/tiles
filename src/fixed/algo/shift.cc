#include "tiles/fixed/algo/shift.h"

namespace tiles {

void shift(fixed_null&, uint32_t const) {}

void shift(fixed_xy& pt, uint32_t const delta_z) {
  pt.x(pt.x() >> delta_z);
  pt.y(pt.y() >> delta_z);
}

template <typename Container>
void shift(Container& c, uint32_t const delta_z) {
  for (auto& e : c) {
    shift(e, delta_z);
  }
}

void shift(fixed_polyline& multi_polyline, uint32_t const delta_z) {
  for (auto& polyline : multi_polyline) {
    shift(polyline, delta_z);
  }
}

void shift(fixed_polygon& multi_polygon, uint32_t const delta_z) {
  for (auto& polygon : multi_polygon) {
    shift(polygon.outer(), delta_z);
    for (auto& ring : polygon.inners()) {
      shift(ring, delta_z);
    }
  }
}

void shift(fixed_geometry& geometry, uint32_t const z) {
  uint32_t delta_z = 20 - z;
  std::visit([&](auto& arg) { shift(arg, delta_z); }, geometry);
}

}  // namespace tiles
