#include "tiles/fixed/algo/simplify.h"

#include "boost/geometry.hpp"

#include "boost/geometry/geometries/register/linestring.hpp"
#include "boost/geometry/geometries/register/multi_linestring.hpp"
#include "boost/geometry/geometries/register/point.hpp"

BOOST_GEOMETRY_REGISTER_POINT_2D(geo::xy<int64_t>, int64_t,  //
                                 cs::cartesian, x_, y_);

BOOST_GEOMETRY_REGISTER_LINESTRING(std::vector<tiles::fixed_xy>);
BOOST_GEOMETRY_REGISTER_MULTI_LINESTRING(
    std::vector<std::vector<tiles::fixed_xy>>);

namespace tiles {

fixed_geometry simplify(fixed_null_geometry const&, uint32_t const) {
  return fixed_null_geometry{};
}

fixed_geometry simplify(fixed_xy const& point, uint32_t const) { return point; }

fixed_geometry simplify(fixed_polyline const& polyline,
                        uint32_t const delta_z) {
  fixed_polyline output;

  for (auto const& line : polyline.geometry_) {
    std::vector<tiles::fixed_xy> output_part;
    boost::geometry::simplify(line, output_part, 1 << delta_z);
    output.geometry_.emplace_back(std::move(output_part));
  }

  return output;
}

fixed_geometry simplify(fixed_polygon const&, uint32_t const) {
  return fixed_null_geometry{};
}

fixed_geometry simplify(fixed_geometry const& geometry, uint32_t const z) {
  uint32_t delta_z = 20 - z;
  return boost::apply_visitor(
      [&](auto& unpacked) { return simplify(unpacked, delta_z); }, geometry);
}

}  // namespace tiles
