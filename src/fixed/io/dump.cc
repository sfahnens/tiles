#include "tiles/fixed/io/dump.h"

namespace tiles {

void dump(fixed_null_geometry const&) {
  std::cout << "null geometry" << std::endl;
}

void dump(fixed_xy const& point) {
  std::cout << "point geometry\n";
  std::cout << "  " << point.x_ << ", " << point.y_ << "\n";
}

void dump(fixed_polyline const& polyline) {
  std::cout << "polyline geometry: " << polyline.geometry_.size() << "\n";
  for (auto i = 0u; i < polyline.geometry_.size(); ++i) {
    for (auto& point : polyline.geometry_[i]) {
      std::cout << "  " << i << "\t" << point.x_ << ", " << point.y_ << "\n";
    }
  }
}

void dump(fixed_polygon const&) {
  std::cout << "polygon geometry\n";
  std::cout << "  not supported\n";
}

void dump(fixed_geometry const& geometry) {
  boost::apply_visitor([](auto& unpacked) { dump(unpacked); }, geometry);
}

}  // namespace tiles
