#include "tiles/osm/feature_handler.h"

#include "sol.hpp"

#include "tiles/db/insert_feature.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/fixed/io/serialize.h"
#include "tiles/osm/pending_feature.h"
#include "tiles/osm/read_osm_geometry.h"

namespace tiles {

struct feature_handler::script_runner {
  script_runner() {
    lua_.script_file("../profile/profile.lua");
    lua_.open_libraries(sol::lib::base, sol::lib::package);

    lua_.new_usertype<pending_feature>(  //
        "pending_feature",  //
        "get_id", &pending_feature::get_id,  //
        "has_tag", &pending_feature::has_tag,  //
        "has_any_tag", &pending_feature::has_any_tag,  //
        "has_any_tag2", &pending_feature::has_any_tag2,  //
        "set_approved", &pending_feature::set_approved,  //
        "set_target_layer", &pending_feature::set_target_layer,  //
        "add_tag_as_metadata", &pending_feature::add_tag_as_metadata);

    process_node_ = lua_["process_node"];
    process_way_ = lua_["process_way"];
    process_area_ = lua_["process_area"];
  }

  sol::state lua_;

  sol::function process_node_;
  sol::function process_way_;
  sol::function process_area_;
};

feature_handler::feature_handler(tile_database& db)
    : runner_(std::make_unique<feature_handler::script_runner>()), db_(db) {}
feature_handler::~feature_handler() = default;

template <typename Object>
std::map<std::string, std::string> make_meta(pending_feature const& f,
                                             Object const& o) {
  std::map<std::string, std::string> meta;

  meta["layer"] = f.target_layer_;

  for (auto const& tag : f.tag_as_metadata_) {
    meta[tag] = std::string{o.get_value_by_key(tag.c_str(), "")};
  }

  return meta;
}

// XXX this is some heavy code duplication

void feature_handler::node(osmium::Node const& node) {
  auto pf = pending_feature{node};
  runner_->process_node_(pf);

  if (!pf.is_approved_[0]) {  // XXX
    return;
  }

  // std::cout << "node " << node.id() << " approved and added" << std::endl;
  insert_feature(db_, feature{make_meta(pf, node), read_osm_geometry(node)});
}

void feature_handler::way(osmium::Way const& way) {
  auto pf = pending_feature{way};
  if (way.nodes().size() < 2) {
    return;  // XXX
  }

  runner_->process_way_(pf);

  if (!pf.is_approved_[0]) {  // XXX
    return;
  }

  // std::cout << "way " << way.id() << " approved and added" << std::endl;
  insert_feature(db_, feature{make_meta(pf, way), read_osm_geometry(way)});
}

void feature_handler::area(osmium::Area const& area) {
  auto pf = pending_feature{area};
  runner_->process_area_(pf);

  if (!pf.is_approved_[0]) {  // XXX
    return;
  }

  // std::cout << "area " << area.id() << " approved and added" << std::endl;
  insert_feature(db_, feature{make_meta(pf, area), read_osm_geometry(area)});
}

}  // namespace tiles
