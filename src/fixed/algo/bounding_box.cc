#include "tiles/fixed/algo/bounding_box.h"

#include "utl/zip.h"

namespace tiles {

fixed_box bounding_box(fixed_xy const& point) {
  return {point.x_, point.y_, point.x_, point.y_};
}

fixed_box bounding_box(fixed_polyline const& polyline) {
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

  return {min_x, min_y, max_x, max_y};
}

fixed_box bounding_box(fixed_polygon const& polygon) {
  auto min_x = kFixedCoordMax;
  auto min_y = kFixedCoordMax;
  auto max_x = kFixedCoordMin;
  auto max_y = kFixedCoordMin;

  verify(polygon.geometry_.size() == polygon.type_.size(), "invalid polygon");
  for (auto const& pair : utl::zip(polygon.geometry_, polygon.type_)) {
    if (!std::get<1>(pair)) {
      continue;
    }

    for (auto const& point : std::get<0>(pair)) {
      min_x = std::min(min_x, point.x_);
      min_y = std::min(min_y, point.y_);
      max_x = std::max(max_x, point.x_);
      max_y = std::max(max_y, point.y_);
    }
  }
  return {min_x, min_y, max_x, max_y};
}

}  // namespace tiles
