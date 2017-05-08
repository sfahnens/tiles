#include "tiles/fixed/clip.h"

namespace tiles {

fixed_geometry clip(fixed_null_geometry const&, tile_spec const&) {
  return fixed_null_geometry{};
}

fixed_geometry clip(fixed_xy const& point, tile_spec const& spec) {
  auto const& box = spec.pixel_bounds_;
  if (point.x_ < box.minx_ || point.y_ < box.miny_ ||  //
      point.x_ > box.maxx_ || point.y_ > box.maxy_) {
    return fixed_null_geometry{};
  } else {
    return point;
  }
}

fixed_geometry clip(fixed_polyline const&, tile_spec const&) {
  return fixed_null_geometry{};
}

fixed_geometry clip(fixed_polygon const&, tile_spec const&) {
  return fixed_null_geometry{};
}

fixed_geometry clip(fixed_geometry const& geometry, tile_spec const& spec) {
  return boost::apply_visitor(
      [&](auto const& unpacked) { return clip(unpacked, spec); }, geometry);
}

}  // namespace tiles
