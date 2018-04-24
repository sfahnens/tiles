#include "tiles/fixed/algo/simplify.h"

// #include "boost/geometry/algorithms/simplify.hpp"
// #include "boost/geometry/strategies/cartesian/distance_pythagoras.hpp"

#include "boost/geometry.hpp"

namespace tiles {

fixed_geometry simplify(fixed_null const& in, uint32_t const) { return in; }

fixed_geometry simplify(fixed_point const& in, uint32_t const) { return in; }

fixed_geometry simplify(fixed_polyline const& in, uint32_t const d) {
  fixed_polyline output;
  boost::geometry::simplify(in, output, d);
  return output;
}

fixed_geometry simplify(fixed_polygon const& in, uint32_t const d) {
  fixed_polygon output;
  boost::geometry::simplify(in, output, d);
  return output;  // XXX check if still valid
}

fixed_geometry simplify(fixed_geometry const& in, uint32_t const z) {
  uint32_t d = 1 << (20 - z);
  return std::visit([&](auto const& arg) { return simplify(arg, d); }, in);
}

}  // namespace tiles
