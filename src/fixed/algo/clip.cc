#include "tiles/fixed/algo/clip.h"

// #include "boost/geometry/algorithms/intersection.hpp"

#include "boost/geometry.hpp"

#include "utl/erase_if.h"

#include "tiles/fixed/io/to_svg.h"
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
  // TODO check if intersection is still broken once boost 1.68 is released!
  using coord_t = double;
  using pt_t = boost::geometry::model::d2::point_xy<coord_t>;
  using polygon_t = boost::geometry::model::multi_polygon<
      boost::geometry::model::polygon<pt_t>>;
  using box_t = boost::geometry::model::box<pt_t>;

  polygon_t in2;
  for (auto const& poly : in) {
    in2.emplace_back();

    for (auto const& pt : poly.outer()) {
      in2.back().outer().emplace_back(static_cast<coord_t>(pt.x()),
                                      static_cast<coord_t>(pt.y()));
    }
    for (auto const& inner : poly.inners()) {
      in2.back().inners().emplace_back();
      for (auto const& pt : inner) {
        in2.back().inners().back().emplace_back(static_cast<coord_t>(pt.x()),
                                                static_cast<coord_t>(pt.y()));
      }
    }
  }

  box_t box2;
  box2.min_corner().x(static_cast<coord_t>(box.min_corner().x()));
  box2.min_corner().y(static_cast<coord_t>(box.min_corner().y()));
  box2.max_corner().x(static_cast<coord_t>(box.max_corner().x()));
  box2.max_corner().y(static_cast<coord_t>(box.max_corner().y()));

  polygon_t out2;
  boost::geometry::intersection(box2, in2, out2);

  fixed_polygon out;
  for (auto const& poly : out2) {
    out.emplace_back();

    for (auto const& pt : poly.outer()) {
      out.back().outer().emplace_back(static_cast<fixed_coord_t>(pt.x()),
                                      static_cast<fixed_coord_t>(pt.y()));
    }
    for (auto const& inner : poly.inners()) {
      out.back().inners().emplace_back();
      for (auto const& pt : inner) {
        out.back().inners().back().emplace_back(
            static_cast<fixed_coord_t>(pt.x()),
            static_cast<fixed_coord_t>(pt.y()));
      }
    }
  }

  if (out.empty()) {
    return fixed_null{};
  } else {
    return out;  // XX what about empty rings?
  }
}

fixed_geometry clip(fixed_geometry const& in, fixed_box const& box) {
  return std::visit([&](auto const& arg) { return clip(arg, box); }, in);
}

}  // namespace tiles
