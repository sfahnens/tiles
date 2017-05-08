#include "tiles/fixed/shift.h"

namespace tiles {

struct shifter : public boost::static_visitor<> {

  void operator()(fixed_null_geometry&, uint32_t const) const {}

  void operator()(fixed_xy& point, uint32_t const delta_z) const {
    point.x_ = point.x_ >> delta_z;
    point.y_ = point.y_ >> delta_z;
  }

  void operator()(fixed_polyline& polyline, uint32_t const delta_z) const {
    for (auto& point : polyline.geometry_) {
      point.x_ = point.x_ >> delta_z;
      point.y_ = point.y_ >> delta_z;
    }
  }

  void operator()(fixed_polygon&, uint32_t const) const {
    // TODO
  }
};

void shift(fixed_geometry& geometry, uint32_t const z) {
  uint32_t delta_z = 20 - z;
  boost::apply_visitor([&](auto& unpacked) { shifter{}(unpacked, delta_z); },
                       geometry);
}

}  // namespace tiles
