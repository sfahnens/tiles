#pragma once

#include "clipper/clipper.hpp"

#include "utl/to_vec.h"
#include "utl/verify.h"

#include "tiles/fixed/convert.h"

namespace tiles {

inline fixed_box bounding_box(ClipperLib::Paths const& geo) {
  auto min_x = std::numeric_limits<fixed_coord_t>::max();
  auto min_y = std::numeric_limits<fixed_coord_t>::max();
  auto max_x = std::numeric_limits<fixed_coord_t>::min();
  auto max_y = std::numeric_limits<fixed_coord_t>::min();

  for (auto const& path : geo) {
    for (auto const& pt : path) {
      min_x = std::min(min_x, pt.X);
      min_y = std::min(min_y, pt.Y);
      max_x = std::max(max_x, pt.X);
      max_y = std::max(max_y, pt.Y);
    }
  }

  return fixed_box{{min_x, min_y}, {max_x, max_y}};
}

inline ClipperLib::Path box_to_path(fixed_box const& box) {
  return {{box.min_corner().x(), box.min_corner().y()},
          {box.max_corner().x(), box.min_corner().y()},
          {box.max_corner().x(), box.max_corner().y()},
          {box.min_corner().x(), box.max_corner().y()}};
}

inline void to_fixed_polygon(ClipperLib::PolyNodes const& nodes,
                             fixed_polygon& polygon) {
  auto const path_to_ring = [](auto const& path) {
    utl::verify(path.size() > 2, "to_fixed_polygon: ring invalid ({} > 2)",
                path.size());
    fixed_ring ring;
    ring.reserve(path.size() + 1);
    for (auto const& pt : path) {
      ring.emplace_back(pt.X, pt.Y);
    }
    ring.emplace_back(path[0].X, path[0].Y);
    return ring;
  };

  for (auto const* outer : nodes) {
    utl::verify(!outer->IsHole(), "outer ring is hole");
    fixed_simple_polygon simple;
    simple.outer() = path_to_ring(outer->Contour);

    for (auto const* inner : outer->Childs) {
      utl::verify(inner->IsHole(), "inner ring is no hole");
      auto inner_ring = path_to_ring(inner->Contour);
      if (!inner_ring.empty()) {
        simple.inners().emplace_back(std::move(inner_ring));
      }

      to_fixed_polygon(inner->Childs, polygon);
    }

    if (!simple.outer().empty()) {
      polygon.emplace_back(std::move(simple));
    }
  }
}

inline void to_clipper_paths(fixed_simple_polygon const& polygon,
                             ClipperLib::Paths& paths) {
  auto const convert_path = [](auto const& in, auto const orientation) {
    auto p = utl::to_vec(in, [](auto const& pt) {
      return ClipperLib::IntPoint{pt.x(), pt.y()};
    });
    if (ClipperLib::Orientation(p) != orientation) {
      ClipperLib::ReversePath(p);
    }
    return p;
  };

  paths.emplace_back(convert_path(polygon.outer(), true));
  for (auto const& inner : polygon.inners()) {
    paths.emplace_back(convert_path(inner, false));
  }
}

inline void to_clipper_paths(fixed_polygon const& multi_polygon,
                             ClipperLib::Paths& paths) {
  for (auto const& polygon : multi_polygon) {
    to_clipper_paths(polygon, paths);
  }
}

inline ClipperLib::Paths intersection(ClipperLib::Paths const& subject,
                                      ClipperLib::Path const& clip) {
  ClipperLib::Clipper clpr;
  utl::verify(clpr.AddPaths(subject, ClipperLib::ptSubject, true),
              "AddPath failed");
  utl::verify(clpr.AddPath(clip, ClipperLib::ptClip, true), "AddPaths failed");

  ClipperLib::Paths solution;
  utl::verify(clpr.Execute(ClipperLib::ctIntersection, solution,
                           ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd),
              "Execute failed");
  return solution;
}

inline std::string to_geojson(ClipperLib::Paths const& paths) {
  std::string buf;
  for (auto i = 0UL; i < paths.size(); ++i) {
    if (i != 0) {
      buf.append(",");
    }
    buf.append("[\n");
    for (auto const& pt : paths[i]) {
      auto ll = fixed_to_latlng({pt.X, pt.Y});
      buf.append(fmt::format("[{}, {}],\n", ll.lng_, ll.lat_));
    }
    if (!paths[i].empty()) {
      auto ll = fixed_to_latlng({paths[i].front().X, paths[i].front().Y});
      buf.append(fmt::format("[{}, {}]\n", ll.lng_, ll.lat_));
    }
    buf.append("]");
  }

  constexpr auto const pattern = R"({{
  "type": "Feature",
  "properties": {{}},
  "geometry": {{
    "type": "Polygon",
    "coordinates": [
  {}       
      ]
    }}
  }}
  )";

  return fmt::format(pattern, buf);
}

inline std::string to_geojson(ClipperLib::PolyTree const& tree) {
  ClipperLib::Paths paths;
  ClipperLib::PolyTreeToPaths(tree, paths);
  return to_geojson(paths);
}

}  // namespace tiles
