#include "tiles/loader/loader.h"

#include "osmium/index/map/dense_file_array.hpp"

#include "geo/webmercator.h"

#include "tiles/database.h"
#include "tiles/slice.h"
#include "tiles/geo/flat_geometry.h"
#include "tiles/geo/flat_point.h"
#include "tiles/geo/pixel32.h"
#include "tiles/osm_util.h"
#include "tiles/util.h"

#include "tiles/loader/pending_feature.h"
#include "tiles/loader/script_runner.h"

using namespace geo;
using namespace rocksdb;
using namespace rocksdb::spatial;

namespace tiles {

int open_file(std::string const& filename) {
  int fd = ::open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) {
    throw std::system_error(
        errno, std::system_category(),
        std::string("Open failed for index '") + filename + "'");
  }
  return fd;
}

struct loader {

  loader()
      : osm_file_("/data/osm/hessen-latest.osm.pbf"),
        node_index_fd_(open_file("nodes.idx")),
        node_index_(node_index_fd_),
        db_(open_spatial_db("spatial")) {}

  ~loader() {
    // XXX clone node_index_fd_ !?
  }

  void load() {}

  void load_nodes() {
    foreach_osm_node(osm_file_, [&](auto const& node) {
      auto const location =
          latlng_to_pixel32({node.location().lat(), node.location().lon()});
      node_index_.set(node.id(), location);

      auto pending = pending_node{node};
      runner_.process_node(pending);

      if (pending.is_approved_[0]) {  // XXX
        std::cout << node.id() << " approved and added" << std::endl;


        FeatureSet feature;
        feature.Set("layer", pending.target_layer_);

        // XXX
        // auto const geometry = make_point(location);

        auto const xy =
            latlng_to_merc({node.location().lat(), node.location().lon()});
        std::vector<flat_geometry> geometry{flat_geometry{feature_type::POINT},
                                            flat_geometry{xy.x_},
                                            flat_geometry{xy.y_}};

        checked(db_->Insert(WriteOptions(), bbox(xy), to_slice(geometry),
                            feature, {"zoom10"}));
      }
    });
  }

  void load_ways() {
    // foreach_osm_way(osm_file_, [&](auto const& way) {
    //   auto pending = pending_way{way};
    //   runner_.process_way(pending);

    //   if (pending.is_approved_) {
    //     // rocksdb
    //   }

    //   // add node location to cache

    // });
  }

  std::string osm_file_;
  script_runner runner_;

  int node_index_fd_;
  osmium::index::map::DenseFileArray<uint64_t, pixel32_xy> node_index_;

  spatial_db_ptr db_;
};

void load() {
  loader l;
  l.load_nodes();
  l.load_ways();
}

}  // namespace tiles
