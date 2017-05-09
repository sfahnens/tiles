#include "tiles/fixed/algo/clip.h"

#include "tiles/util.h"

namespace tiles {

fixed_geometry clip(fixed_null_geometry const&, tile_spec const&) {
  return fixed_null_geometry{};
}

bool within(fixed_xy const& point, geo::pixel_bounds const& box) {
  return point.x_ >= box.minx_ && point.y_ >= box.miny_ &&  //
         point.x_ <= box.maxx_ && point.y_ <= box.maxy_;
}

fixed_geometry clip(fixed_xy const& point, tile_spec const& spec) {
  if (within(point, spec.pixel_bounds_)) {
    return point;
  } else {
    return fixed_null_geometry{};
  }
}

fixed_geometry clip(fixed_polyline const& polyline, tile_spec const& spec) {
  auto const& box = spec.pixel_bounds_;

  std::vector<std::vector<fixed_xy>> result;
  result.emplace_back();

  auto const draw_within = [&](auto const& line, auto i) {
    if (result.back().empty()) {
      result.back().emplace_back(line[i - 1]);
    }

    result.back().emplace_back(line[i]);
  };

  // see: http://www.skytopia.com/project/articles/compsci/clipping.htm
  auto const draw_liang_barsky = [&](auto const& line, auto i) {
    auto x0 = static_cast<int64_t>(line[i - 1].x_);
    auto y0 = static_cast<int64_t>(line[i - 1].y_);
    auto x1 = static_cast<int64_t>(line[i].x_);
    auto y1 = static_cast<int64_t>(line[i].y_);

    auto t0 = 0.0;
    auto t1 = 1.0;
    auto dx = x1 - x0;
    auto dy = y1 - y0;
    int64_t p, q;
    double r;

    for (auto edge = 0u; edge < 4; ++edge) {
      if (edge == 0) {
        p = -dx;
        q = -(box.minx_ - x0);
      }
      if (edge == 1) {
        p = dx;
        q = (box.maxx_ - x0);
      }
      if (edge == 2) {
        p = -dy;
        q = -(box.miny_ - y0);
      }
      if (edge == 3) {
        p = dy;
        q = (box.maxy_ - y0);
      }

      if (p == 0 && q < 0) {  // parallel line outside
        return false;
      }

      r = static_cast<double>(q) / static_cast<double>(p);
      if (p < 0) {
        if (r > t1) {
          return false;
        } else if (r > t0) {
          t0 = r;
        }
      } else if (p > 0) {
        if (r < t0) {
          return false;
        } else if (r < t1) {
          t1 = r;
        }
      }
    }

    // std::cout << t0 << " " << t1 << std::endl;

    if (result.back().empty()) {
      result.back().emplace_back(x0 + t0 * dx, y0 + t0 * dy);
    }

    result.back().emplace_back(x0 + t1 * dx, y0 + t1 * dy);

    return true;
  };

  verify(polyline.geometry_.size() == 1, "unsupported geometry");
  verify(polyline.geometry_[0].size() > 1, "line to short to clip");
  auto const& line = polyline.geometry_[0];

  auto last_within = within(line.front(), box);

  for (auto i = 1u; i < line.size(); ++i) {
    auto curr_within = within(line[i], box);

    if (last_within && curr_within) {
      // std::cout << "full within" << std::endl;
      draw_within(line, i);
    } else {
      if (draw_liang_barsky(line, i) && !curr_within) {
        result.emplace_back();
      }
    }
    last_within = curr_within;
  }

  // std::cout << result.size() << std::endl;

  if (result.back().empty()) {
    result.erase(std::next(end(result), -1));
  }

  if (result.empty()) {
    return fixed_null_geometry{};
  } else {
    return fixed_polyline{std::move(result)};
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
