#include "tiles/fixed/algo/simplify.h"

#include "boost/geometry.hpp"

#include "utl/erase_if.h"

#include "tiles/util.h"

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

  // triangle + closing point -> 4
  utl::erase_if(output, [](auto& poly) {
    utl::erase_if(poly.inners(),
                  [](auto const& inner) { return inner.size() < 4; });

    return poly.outer().size() < 4;
  });

  if (output.empty()) {
    return fixed_null{};
  } else if (!boost::geometry::is_valid(output)) {
    // no simplification is inefficient ...
    // ... but since boost::geometry::dissolve is broken
    return in;
  }
  return output;
}

fixed_geometry simplify(fixed_geometry const& in, uint32_t const z) {
  uint32_t d = 1 << (20 - z);
  return std::visit([&](auto const& arg) { return simplify(arg, d); }, in);
}

}  // namespace tiles
