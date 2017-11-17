#include "tiles/fixed/algo/bounding_box.h"

#include "utl/zip.h"

#include "boost/geometry.hpp"

namespace tiles {

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

}  // namespace tiles
