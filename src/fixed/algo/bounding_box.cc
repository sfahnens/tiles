#include "tiles/fixed/algo/bounding_box.h"

namespace tiles {

using bbox = rocksdb::spatial::BoundingBox<double>;

bbox bounding_box(fixed_xy const& point) {
  return {static_cast<double>(point.x_), static_cast<double>(point.y_),
          static_cast<double>(point.x_), static_cast<double>(point.y_)};
}

bbox bounding_box(fixed_polyline const& polyline) {
  auto min_x = kFixedCoordMax;
  auto min_y = kFixedCoordMax;
  auto max_x = kFixedCoordMin;
  auto max_y = kFixedCoordMin;

  for (auto const& line : polyline.geometry_) {
    for (auto const& point : line) {
      min_x = std::min(min_x, point.x_);
      min_y = std::min(min_y, point.y_);
      max_x = std::max(max_x, point.x_);
      max_y = std::max(max_y, point.y_);
    }
  }

  // std::cout << min_x << " " << max_x << " " << min_y << " " << max_y <<
  // std::endl;

  return {static_cast<double>(min_x), static_cast<double>(min_y),
          static_cast<double>(max_x), static_cast<double>(max_y)};
}

}  // namespace tiles
