#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>

#include "google/dense_hash_map"

#include "geo/webmercator.h"
#include "tiles/loader/osm_util.h"

#include <osmium/index/map/dense_file_array.hpp>

using namespace tiles;
using namespace geo;

using proj = webmercator<4096, 20>;

using pixel32_t = uint32_t;
using pixel32_xy = xy<pixel32_t>;

pixel32_xy latlng_to_pixel32(latlng const& pos) {
  auto const px = proj::merc_to_pixel(latlng_to_merc(pos), proj::kMaxZoomLevel);
  constexpr int64_t kMax = std::numeric_limits<uint32_t>::max();
  return {static_cast<pixel32_t>(std::min(px.x_, kMax)),
          static_cast<pixel32_t>(std::min(px.y_, kMax))};
}

using file_index = osmium::index::map::DenseFileArray<uint64_t, pixel32_xy>;

int main() {
  int64_t counter = 0;
  int64_t max = -1;

  int fd = open("densefile.index", O_RDWR | O_CREAT, 0666);
  if (fd == -1) {
    std::cerr << "Can not open index file"
              << ": " << strerror(errno) << "\n";
    return 1;
  }

  file_index idx{fd};

  foreach_osm_node("/data/osm/planet-latest.osm.pbf", [&](auto const& node) {
    ++counter;
    max = std::max(max, node.id());

    idx.set(node.id(),
            latlng_to_pixel32({node.location().lat(), node.location().lon()}));
  });

  // std::cout << "node count " << counter << std::endl;
  // std::cout << "max nodeid " << max << std::endl;

  // std::map<long, size_t> delta_idx_map_;
  std::map<long, size_t> delta_coord_map_;

  size_t dist_counter = 0;
  size_t sixteen_counter = 0;
  size_t fourteen_counter = 0;
  size_t six_counter = 0;

  foreach_osm_way("/data/osm/planet-latest.osm.pbf", [&](auto const& way) {
    auto const& nodes = way.nodes();

    for (auto i = 1u; i < nodes.size(); ++i) {
      auto const idx_0 = nodes[i - 1].ref();
      auto const idx_1 = nodes[i].ref();

      // ++delta_idx_map_[static_cast<long>(idx_1) - static_cast<long>(idx_0)];

      auto const pos_0 = idx.get(idx_0);
      auto const pos_1 = idx.get(idx_1);

      dist_counter += 2;

      auto const dx =
          std::abs(static_cast<long>(pos_1.x_) - static_cast<long>(pos_0.x_));
      if (dx < 2 << (16 - 1)) {
        ++sixteen_counter;
      }

      if (dx < 2 << (14 - 1)) {
        ++fourteen_counter;
      }

      if (dx < 2 << (6 - 1)) {
        ++six_counter;
      }

      auto const dy =
          std::abs(static_cast<long>(pos_1.y_) - static_cast<long>(pos_0.y_));
      if (dy < 2 << (16 - 1)) {
        ++sixteen_counter;
      }

      if (dy < 2 << (14 - 1)) {
        ++fourteen_counter;
      }

      if (dy < 2 << (6 - 1)) {
        ++six_counter;
      }

      // ++delta_coord_map_[static_cast<long>(pos_1.x_) -
      // static_cast<long>(pos_0.x_)];
      // ++delta_coord_map_[static_cast<long>(pos_1.y_) -
      // static_cast<long>(pos_0.y_)];
    }
  });

  std::cout << "distances\t:" << std::setw(22) << dist_counter << std::endl;
  std::cout << "-  16 bit\t:" << std::setw(22) << sixteen_counter << std::endl;
  std::cout << "-  14 bit\t:" << std::setw(22) << fourteen_counter << std::endl;
  std::cout << "-   6 bit\t:" << std::setw(22) << six_counter << std::endl;

  // auto const hist =[](std::string const& name, std::map<long, size_t> const&
  // map) {
  //   // std::cout << "histogram: " << name << std::endl;
  //   for(auto const& pair : map) {
  //     std::cout << pair.first << ", " << pair.second << "\n";
  //   }
  //   // std::cout << "===========" << std::endl;
  // };

  // // hist("delta idx", delta_idx_map_);
  // hist("delta", delta_coord_map_);

  // std::sort(begin(vec), end(vec));

  // std::map<int64_t, size_t> hist;
  // for(auto i = 1ul; i < vec.size(); ++i) {
  //   ++hist[vec[i] - vec[i-1]];
  // }

  // std::ofstream out{"delta.histogram"};
  // for(auto const& pair : hist) {
  //   out << pair.first << "\t" << pair.second << "\n";
  // }

  //   std::cout << "max lat: " << max_lat << std::endl;

  // constexpr auto z = 20;

  // auto topleft = latlng_to_merc({kMaxLat, -180});

  // std::cout << proj::merc_to_pixel_x(topleft.x_, z) << ", "
  //           << proj::merc_to_pixel_y(topleft.y_, z) << std::endl;

  // // auto botright = latlng_to_merc({-kMaxLat, 180});

  // auto botright = latlng_to_pixel32({-kMaxLat, 180});

  // std::cout << botright.x_ << ", " << botright.y_ << std::endl;

  // // uint32_t x =
  // //     std::min(proj::merc_to_pixel_x(botright.x_, z),
  // // static_cast<int64_t>(std::numeric_limits<uint32_t>::max()));

  // // std::cout << x << ", " << proj::merc_to_pixel_y(botright.y_, z) <<
  // // std::endl;
}
