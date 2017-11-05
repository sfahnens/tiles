// #include "tiles/loader/loader.h"

// #include "osmium/index/map/dense_file_array.hpp"

// #include "geo/webmercator.h"

// #include "utl/to_vec.h"

// #include "tiles/database.h"

// #include "tiles/fixed/algo/bounding_box.h"
// #include "tiles/fixed/fixed_geometry.h"
// #include "tiles/fixed/io/serialize.h"

// #include "tiles/util.h"

// #include "tiles/loader/osm_util.h"
// #include "tiles/loader/pending_feature.h"
// #include "tiles/loader/script_runner.h"

// using namespace geo;
// using namespace rocksdb;
// using namespace rocksdb::spatial;

// namespace tiles {

// int open_file(std::string const& filename) {
//   int fd = ::open(filename.c_str(), O_RDWR /*| O_CREAT | O_TRUNC*/, 0666);
//   // int fd = ::open(filename.c_str(), O_RDWR | O_CREAT /*| O_TRUNC*/, 0666);
//   if (fd < 0) {
//     throw std::system_error(
//         errno, std::system_category(),
//         std::string("Open failed for index '") + filename + "'");
//   }
//   return fd;
// }

// struct loader {

//   loader()
//       : osm_file_("/data/osm/hessen-latest.osm.pbf"),
//         node_index_fd_(open_file("nodes.idx")),
//         node_index_(node_index_fd_),
//         db_(open_spatial_db("spatial")) {}

//   ~loader() {
//     // XXX clone node_index_fd_ !?
//     close(node_index_fd_);
//   }

//   void load() {}

  // void load_nodes() {
  //   foreach_osm_node(osm_file_, [&](auto const& node) {
  //     auto const location =
  //         latlng_to_fixed({node.location().lat(), node.location().lon()});

  //     // node_index_.set(node.id(), location);

  //     auto pending = pending_node{node};
  //     runner_.process_node(pending);

  //     if (pending.is_approved_[0]) {  // XXX
  //       std::cout << "node " << node.id() << " approved and added" << std::endl;

  //       FeatureSet feature;
  //       feature.Set("layer", pending.target_layer_);

  //       for (auto const& tag : pending.tag_as_metadata_) {
  //         feature.Set(tag, std::string{node.get_value_by_key(tag.c_str(), "")});
  //       }

  //       auto const string = serialize(location);

  //       checked(db_->Insert(WriteOptions(), bounding_box(location),
  //                           Slice(string), feature, {"zoom10"}));
  //     }
  //   });
  // }

//   void load_ways() {
//     foreach_osm_way(osm_file_, [&](auto const& way) {
//       auto pending = pending_way{way};

//       if (way.nodes().size() < 2) {
//         return;  // XXX
//       }

//       runner_.process_way(pending);

//       if (pending.is_approved_[0]) {
//         std::cout << "way " << way.id() << " approved and added" << std::endl;

//         FeatureSet feature;
//         feature.Set("layer", pending.target_layer_);

//         fixed_polyline polyline;
//         polyline.geometry_.emplace_back(
//             utl::to_vec(way.nodes(), [this](auto const& node_ref) {
//               return node_index_.get(node_ref.ref());
//             }));

//         // TODO verify that distances fit into int32_t (or clipping will not
//         // work)

//         auto const string = serialize(polyline);

//         checked(db_->Insert(WriteOptions(), bounding_box(polyline),
//                             Slice(string), feature, {"zoom10"}));
//       }
//     });
//   }

//   std::string osm_file_;
//   script_runner runner_;

//   int node_index_fd_;
//   osmium::index::map::DenseFileArray<uint64_t, fixed_xy> node_index_;
//   // osmium::index::map::DenseFileArray<uint64_t, pixel32_xy> node_index_;

//   spatial_db_ptr db_;
// };

// void load() {
//   loader l;
//   std::cout << "p1: nodes" << std::endl;
//   l.load_nodes();
//   std::cout << "p2: ways" << std::endl;
//   l.load_ways();
//   std::cout << "p3: done" << std::endl;
// }

// }  // namespace tiles
