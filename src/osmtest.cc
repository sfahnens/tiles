// #include <algorithm>
// #include <fstream>
// #include <limits>

// #include "google/dense_hash_map"

// #include "geo/webmercator.h"
// #include "tiles/osm_util.h"

// #include <osmium/index/map/dense_file_array.hpp>

// using namespace tiles;
// using namespace geo;

// using proj = webmercator<4096, 20>;

// using pixel32_t = uint32_t;
// using pixel32_xy = xy<pixel32_t>;

// pixel32_xy latlng_to_pixel32(latlng const& pos) {
//   auto const px = proj::merc_to_pixel(latlng_to_merc(pos), proj::kMaxZoomLevel);
//   constexpr int64_t kMax = std::numeric_limits<uint32_t>::max();
//   return {static_cast<pixel32_t>(std::min(px.x_, kMax)),
//           static_cast<pixel32_t>(std::min(px.y_, kMax))};
// }

// using file_index = osmium::index::map::DenseFileArray<uint64_t, pixel32_xy>;

int main() {
//   // auto node_cache = google::dense_hash_map<int64_t, pixel32_xy>{};
//   // node_cache.set_empty_key(std::numeric_limits<int64_t>::min());

//   int64_t counter = 0;

//   int64_t max = -1;

//   // std::vector<int64_t> vec;

//   // int fd = open("densefile.index", O_RDWR | O_CREAT, 0666);
//   // if (fd == -1) {
//   //   std::cerr << "Can not open index file"
//   //             << ": " << strerror(errno) << "\n";
//   //   return 1;
//   // }

//   // file_index idx{fd};

//   foreach_osm_node("/data/osm/planet-latest.osm.pbf", [&](auto const& node) {
//     // TODO node callback

//     // node_cache[node.id()] =
//     //     latlng_to_pixel32({node.location().lat(), node.location().lon()});

//     ++counter;
//     max = std::max(max, node.id());
//     // vec.push_back(node.id());

//     arr[node.id()] =
//         latlng_to_pixel32({node.location().lat(), node.location().lon()});

//     // idx.set(node.id(),
//     //         latlng_to_pixel32({node.location().lat(),
//     //         node.location().lon()}));
//   });

//   std::cout << "node count " << counter << std::endl;
//   std::cout << "max nodeid " << max << std::endl;

//   // std::sort(begin(vec), end(vec));

//   // std::map<int64_t, size_t> hist;
//   // for(auto i = 1ul; i < vec.size(); ++i) {
//   //   ++hist[vec[i] - vec[i-1]];
//   // }

//   // std::ofstream out{"delta.histogram"};
//   // for(auto const& pair : hist) {
//   //   out << pair.first << "\t" << pair.second << "\n";
//   // }

//   //   std::cout << "max lat: " << max_lat << std::endl;

//   // constexpr auto z = 20;

//   // auto topleft = latlng_to_merc({kMaxLat, -180});

//   // std::cout << proj::merc_to_pixel_x(topleft.x_, z) << ", "
//   //           << proj::merc_to_pixel_y(topleft.y_, z) << std::endl;

//   // // auto botright = latlng_to_merc({-kMaxLat, 180});

//   // auto botright = latlng_to_pixel32({-kMaxLat, 180});

//   // std::cout << botright.x_ << ", " << botright.y_ << std::endl;

//   // // uint32_t x =
//   // //     std::min(proj::merc_to_pixel_x(botright.x_, z),
//   // // static_cast<int64_t>(std::numeric_limits<uint32_t>::max()));

//   // // std::cout << x << ", " << proj::merc_to_pixel_y(botright.y_, z) <<
//   // // std::endl;
}
