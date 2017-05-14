#include "tiles/fixed/algo/clip.h"

#include <vector>

#include "boost/geometry.hpp"

#include "boost/geometry/geometries/register/box.hpp"
#include "boost/geometry/geometries/register/linestring.hpp"
#include "boost/geometry/geometries/register/multi_linestring.hpp"
#include "boost/geometry/geometries/register/point.hpp"

#include "tiles/util.h"

BOOST_GEOMETRY_REGISTER_POINT_2D(geo::xy<int64_t>, int64_t,  //
                                 cs::cartesian, x_, y_);

BOOST_GEOMETRY_REGISTER_LINESTRING(std::vector<tiles::fixed_xy>);
BOOST_GEOMETRY_REGISTER_MULTI_LINESTRING(
    std::vector<std::vector<tiles::fixed_xy>>);

BOOST_GEOMETRY_REGISTER_BOX_2D_4VALUES(geo::pixel_bounds, geo::pixel_xy,  //
                                       minx_, miny_, maxx_, maxy_);

namespace tiles {

fixed_geometry clip(fixed_null_geometry const&, tile_spec const&) {
  return fixed_null_geometry{};
}

bool within(fixed_xy const& point, geo::pixel_bounds const& box) {
  return point.x_ >= box.minx_ && point.y_ >= box.miny_ &&  //
         point.x_ <= box.maxx_ && point.y_ <= box.maxy_;
}

fixed_geometry clip(fixed_xy const& point, tile_spec const& spec) {
  if (within(point, spec.overdraw_bounds_)) {
    return point;
  } else {
    return fixed_null_geometry{};
  }
}

fixed_geometry clip(fixed_polyline const& polyline, tile_spec const& spec) {
  fixed_polyline output;
  boost::geometry::intersection(spec.overdraw_bounds_,  //
                                polyline.geometry_,  //
                                output.geometry_);

  if (output.geometry_.empty() ||
      std::all_of(begin(output.geometry_), end(output.geometry_),
                  [](auto const& geo) { return geo.empty(); })) {
    return fixed_null_geometry{};
  } else {
    return output;
  }
}

fixed_geometry clip(fixed_polygon const&, tile_spec const&) {
  return fixed_null_geometry{};
}

fixed_geometry clip(fixed_geometry const& geometry, tile_spec const& spec) {
  return boost::apply_visitor(
      [&](auto const& unpacked) { return clip(unpacked, spec); }, geometry);
}

}  // namespace tiles
