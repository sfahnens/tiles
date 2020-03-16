#include "tiles/fixed/io/dump.h"

namespace tiles {

void dump(fixed_null const&) { std::cout << "null geometry" << std::endl; }

void dump(fixed_point const& geo) {
  std::cout << "point geometry: " << geo.size() << "\n";
  for (auto const& pt : geo) {
    std::cout << "  " << pt.x() << "," << pt.y() << "\n";
  }
}

void dump(fixed_polyline const& geo) {
  std::cout << "polyline geometry: " << geo.size() << "\n";
  for (auto i = 0u; i < geo.size(); ++i) {
    std::cout << "  " << i << " ";
    for (auto const& pt : geo[i]) {
      std::cout << pt.x() << "," << pt.y() << " ";
    }
    std::cout << std::endl;
  }
}

void dump(fixed_polygon const& geo) {
  std::cout << "polygon geometry: " << geo.size() << "\n";
  for (auto i = 0u; i < geo.size(); ++i) {
    std::cout << "## " << i << " o ";
    for (auto const& pt : geo[i].outer()) {
      std::cout << pt.x() << "," << pt.y() << " ";
    }
    std::cout << std::endl;

    for (auto j = 0u; j < geo[i].inners().size(); ++j) {
      std::cout << "## " << i << " " << j << " ";
      for (auto const& pt : geo[i].outer()) {
        std::cout << pt.x() << "," << pt.y() << " ";
      }
      std::cout << std::endl;
    }
  }
}

void dump(fixed_geometry const& geometry) {
  mpark::visit([](auto& arg) { dump(arg); }, geometry);
}

}  // namespace tiles
