#include "tiles/fixed/algo/bounding_box.h"

#include "boost/geometry.hpp"

namespace tiles {

fixed_box bounding_box(fixed_null const&) { return fixed_box{}; }

fixed_box bounding_box(fixed_point const& in) {
  fixed_box box;
  boost::geometry::envelope(in, box);
  return box;
}

fixed_box bounding_box(fixed_polyline const& in) {
  fixed_box box;
  boost::geometry::envelope(in, box);
  return box;
}

fixed_box bounding_box(fixed_polygon const& in) {
  fixed_box box;
  boost::geometry::envelope(in, box);
  return box;
}

fixed_box bounding_box(fixed_simple_polygon const& in) {
  fixed_box box;
  boost::geometry::envelope(in, box);
  return box;
}

fixed_box bounding_box(fixed_geometry const& in) {
  return mpark::visit([&](auto const& arg) { return bounding_box(arg); }, in);
}

}  // namespace tiles
