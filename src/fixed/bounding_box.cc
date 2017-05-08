#include "tiles/fixed/bounding_box.h"

namespace tiles {

using bbox = rocksdb::spatial::BoundingBox<double>;

// struct bounding_box_calculator : public boost::static_visitor<bbox> {

//   bbox operator()(fixed_null_geometry const&) const { return {0, 0, 0, 0}; }

//   bbox operator()(fixed_xy const point) const {
//     return {point.x_, point.y_, point.x_, point.y_};
//   }

//   bbox operator()(fixed_polyline const polyline) const {
//     auto min_x = kFixedCoordMax;
//     auto min_y = kFixedCoordMax;
//     auto max_x = kFixedCoordMin;
//     auto max_y = kFixedCoordMin;

//     for (auto const& point : polyline.geometry_) {
//       min_x = std::min(min_x, point.x_);
//       min_y = std::min(min_y, point.y_);
//       max_x = std::max(max_x, point.x_);
//       max_y = std::max(max_y, point.y_);
//     }

//     return {min_x, min_y, max_x, max_y};
//   }
// };

// bbox bounding_box(fixed_geometry const& geometry) {
//   bounding_box_calculator calc;
//   return boost::apply_visitor(calc, geometry);
// }

bbox bounding_box(fixed_xy const& point) {
  return {static_cast<double>(point.x_), static_cast<double>(point.y_),
          static_cast<double>(point.x_), static_cast<double>(point.y_)};
}

bbox bounding_box(fixed_polyline const& polyline) {
  auto min_x = kFixedCoordMax;
  auto min_y = kFixedCoordMax;
  auto max_x = kFixedCoordMin;
  auto max_y = kFixedCoordMin;

  for (auto const& point : polyline.geometry_) {
    min_x = std::min(min_x, point.x_);
    min_y = std::min(min_y, point.y_);
    max_x = std::max(max_x, point.x_);
    max_y = std::max(max_y, point.y_);
  }

  std::cout << min_x << " " << max_x << " " << min_y << " " << max_y << std::endl;

  return {static_cast<double>(min_x), static_cast<double>(min_y),
          static_cast<double>(max_x), static_cast<double>(max_y)};
}

}  // namespace tiles
