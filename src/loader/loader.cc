#include "tiles/loader/loader.h"

#include "osmium/index/map/dense_file_array.hpp"

#include "geo/webmercator.h"

#include "tiles/database.h"
#include "tiles/geo/flat_geometry.h"
#include "tiles/geo/flat_point.h"
#include "tiles/geo/pixel32.h"
#include "tiles/slice.h"
#include "tiles/util.h"

#include "tiles/loader/osm_util.h"
#include "tiles/loader/pending_feature.h"
#include "tiles/loader/script_runner.h"

using namespace geo;
using namespace rocksdb;
using namespace rocksdb::spatial;

namespace tiles {

int open_file(std::string const& filename) {
  int fd = ::open(filename.c_str(), O_RDWR /*| O_CREAT | O_TRUNC*/, 0666);
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
    close(node_index_fd_);
  }

  void load() {}

  void load_nodes() {
    foreach_osm_node(osm_file_, [&](auto const& node) {
      // auto const location =
      //     latlng_to_pixel32({node.location().lat(), node.location().lon()});

      auto const location =
          latlng_to_merc({node.location().lat(), node.location().lon()});
      // node_index_.set(node.id(), location);

      auto pending = pending_node{node};
      runner_.process_node(pending);

      if (pending.is_approved_[0]) {  // XXX
        std::cout << "node " << node.id() << " approved and added" << std::endl;

        FeatureSet feature;
        feature.Set("layer", pending.target_layer_);

        for(auto const& tag : pending.tag_as_metadata_) {
          feature.Set(tag, std::string{node.get_value_by_key(tag.c_str(), "")});
        }


        // XXX
        // auto const geometry = make_point(location);

        std::vector<flat_geometry> geometry{flat_geometry{feature_type::POINT},
                                            flat_geometry{location.x_},
                                            flat_geometry{location.y_}};

        checked(db_->Insert(WriteOptions(), bbox(location), to_slice(geometry),
                            feature, {"zoom10"}));
      }
    });
  }

  void load_ways() {
    foreach_osm_way(osm_file_, [&](auto const& way) {
      auto pending = pending_way{way};
      runner_.process_way(pending);

      if (pending.is_approved_[0]) {
        std::cout << "way " << way.id() << " approved and added" << std::endl;

        FeatureSet feature;
        feature.Set("layer", pending.target_layer_);

        double minx = std::numeric_limits<double>::infinity();
        double miny = std::numeric_limits<double>::infinity();
        double maxx = -std::numeric_limits<double>::infinity();
        double maxy = -std::numeric_limits<double>::infinity();

        auto const& nodes = way.nodes();

        std::vector<flat_geometry> mem{
            flat_geometry{feature_type::POLYLINE, nodes.size()}};

        for (auto const& node_ref : nodes) {
          auto const xy = node_index_.get(node_ref.ref());

          // auto const xy = latlng_to_merc(pos);
          mem.push_back(flat_geometry{xy.x_});
          mem.push_back(flat_geometry{xy.y_});

          minx = xy.x_ < minx ? xy.x_ : minx;
          miny = xy.y_ < miny ? xy.y_ : miny;
          maxx = xy.x_ > maxx ? xy.x_ : maxx;
          maxy = xy.y_ > maxy ? xy.y_ : maxy;
        }

        checked(db_->Insert(WriteOptions(), {minx, miny, maxx, maxy},
                           to_slice(mem), feature, {"zoom10"}));
      }
    });
  }

  std::string osm_file_;
  script_runner runner_;

  int node_index_fd_;
  osmium::index::map::DenseFileArray<uint64_t, merc_xy> node_index_;
  // osmium::index::map::DenseFileArray<uint64_t, pixel32_xy> node_index_;

  spatial_db_ptr db_;
};

void load() {
  loader l;
  std::cout << "p1: nodes" << std::endl;
  l.load_nodes();
  std::cout << "p2: ways" << std::endl;
  // l.load_ways();
  std::cout << "p3: done" << std::endl;
}

}  // namespace tiles
