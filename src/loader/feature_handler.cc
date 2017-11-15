#include "tiles/loader/feature_handler.h"

#include "sol.hpp"

#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/fixed/io/serialize.h"
#include "tiles/loader/convert_osm_geometry.h"
#include "tiles/loader/pending_feature.h"
#include "tiles/tile_spec.h"

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

void feature_handler::node(osmium::Node const& node) {
  auto feature = pending_feature{node};
  runner_->process_node_(feature);

  if (!feature.is_approved_[0]) {  // XXX
    return;
  }

  // std::cout << "node " << node.id() << " approved and added" << std::endl;

  rocksdb::spatial::FeatureSet fs;
  fs.Set("layer", feature.target_layer_);

  for (auto const& tag : feature.tag_as_metadata_) {
    fs.Set(tag, std::string{node.get_value_by_key(tag.c_str(), "")});
  }

  auto const converted = convert_osm_geometry(node);

  rocksdb::spatial::BoundingBox<double> box{
      static_cast<double>(converted.first.min_x_),
      static_cast<double>(converted.first.min_y_),
      static_cast<double>(converted.first.max_x_),
      static_cast<double>(converted.first.max_y_)};

  db_.put_feature(box, rocksdb::Slice(converted.second), fs,
                  tile_spec::zoom_level_names());
}

void feature_handler::way(osmium::Way const& way) {
  auto feature = pending_feature{way};
  if (way.nodes().size() < 2) {
    return;  // XXX
  }

  runner_->process_way_(feature);

  if (!feature.is_approved_[0]) {  // XXX
    return;
  }

  // std::cout << "way " << way.id() << " approved and added" << std::endl;

  rocksdb::spatial::FeatureSet fs;
  fs.Set("layer", feature.target_layer_);

  auto const converted = convert_osm_geometry(way);

  rocksdb::spatial::BoundingBox<double> box{
      static_cast<double>(converted.first.min_x_),
      static_cast<double>(converted.first.min_y_),
      static_cast<double>(converted.first.max_x_),
      static_cast<double>(converted.first.max_y_)};

  db_.put_feature(box, rocksdb::Slice(converted.second), fs,
                  tile_spec::zoom_level_names());
}

void feature_handler::area(osmium::Area const& area) {
  auto feature = pending_feature{area};
  runner_->process_area_(feature);

  if (!feature.is_approved_[0]) {  // XXX
    return;
  }

  rocksdb::spatial::FeatureSet fs;
  fs.Set("layer", feature.target_layer_);

  for(auto const& tag : feature.tag_as_metadata_) {
    std::string const& meta = area.get_value_by_key(tag.c_str(), "");
    // std::cout << tag << " -> " << meta << std::endl;
    fs.Set(tag, meta); // XXX
  }

  // std::cout << "area " << area.id() << " approved and added" << std::endl;

  auto const converted = convert_osm_geometry(area);

  rocksdb::spatial::BoundingBox<double> box{
      static_cast<double>(converted.first.min_x_),
      static_cast<double>(converted.first.min_y_),
      static_cast<double>(converted.first.max_x_),
      static_cast<double>(converted.first.max_y_)};

  db_.put_feature(box, rocksdb::Slice(converted.second), fs,
                  tile_spec::zoom_level_names());
}

}  // namespace tiles
