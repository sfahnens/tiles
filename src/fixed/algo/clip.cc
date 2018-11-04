#include "tiles/fixed/algo/clip.h"

// #include "boost/geometry/algorithms/intersection.hpp"

#include "boost/geometry.hpp"

#include "utl/erase_if.h"

#include "tiles/util.h"

namespace tiles {

fixed_geometry clip(fixed_null const&, fixed_box const&) {
  return fixed_null{};
}

fixed_geometry clip(fixed_point const& in, fixed_box const& box) {
  fixed_point out;

  for (auto const& point : in) {
    if (boost::geometry::within(point, box)) {
      out.push_back(point);
    }
  }

  if (out.empty()) {
    return fixed_null{};
  } else {
    return out;
  }
}

fixed_geometry clip(fixed_polyline const& in, fixed_box const& box) {
  fixed_polyline out;
  boost::geometry::intersection(box, in, out);

  utl::erase_if(out, [](auto const& line) { return line.size() < 2; });
  if (out.empty()) {
    return fixed_null{};
  } else {
    return out;
  }
}

fixed_geometry clip(fixed_polygon const& in, fixed_box const& box) {
  fixed_polygon out;
  boost::geometry::intersection(box, in, out);

  if (out.empty()) {
    return fixed_null{};
  } else {
    return out;  // XX what about empty rings?
  }
}

fixed_geometry clip(fixed_geometry const& in, fixed_box const& box) {
  return mpark::visit([&](auto const& arg) { return clip(arg, box); }, in);
}

}  // namespace tiles
