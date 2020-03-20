#include "tiles/osm/feature_handler.h"

#include "sol.hpp"

#include "tiles/db/feature_inserter_mt.h"
#include "tiles/db/layer_names.h"
#include "tiles/db/shared_metadata.h"
#include "tiles/fixed/fixed_geometry.h"
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
        "has_any_tag", &pending_feature::has_any_tag,  //
        "set_approved_min", &pending_feature::set_approved_min,  //
        "set_approved_min_by_area", &pending_feature::set_approved_min_by_area,
        "set_approved_full", &pending_feature::set_approved_full,  //
        "set_target_layer", &pending_feature::set_target_layer,  //
        "add_tag_as_metadata", &pending_feature::add_tag_as_metadata,  //
        "add_metadata", &pending_feature::add_metadata);

    process_node_ = lua_["process_node"];
    process_way_ = lua_["process_way"];
    process_area_ = lua_["process_area"];
  }

  sol::state lua_;

  sol::function process_node_;
  sol::function process_way_;
  sol::function process_area_;
};

feature_handler::feature_handler(
    feature_inserter_mt& inserter, layer_names_builder& layer_names_builder,
    shared_metadata_builder& shared_metadata_builder)
    : runner_{std::make_unique<feature_handler::script_runner>()},
      inserter_{inserter},
      layer_names_builder_{layer_names_builder},
      shared_metadata_builder_{shared_metadata_builder} {}

feature_handler::feature_handler(feature_handler&&) noexcept = default;
feature_handler::~feature_handler() = default;

template <typename OSMObject>
void handle_feature(feature_inserter_mt& inserter,
                    layer_names_builder& layer_names,
                    shared_metadata_builder& shared_metadata_builder,
                    sol::function const& process, OSMObject const& obj) {
  auto pf = pending_feature{obj, [&obj] { return read_osm_geometry(obj); }};
  process(pf);

  if (!pf.is_approved_) {
    return;
  }

  if (!pf.geometry_) {
    pf.geometry_ = read_osm_geometry(obj);
  }
  if (mpark::holds_alternative<fixed_null>(*pf.geometry_)) {
    return;
  }

  pf.finish_metadata();
  shared_metadata_builder.update(pf.metadata_);

  inserter.insert(feature{static_cast<uint64_t>(pf.get_id()),
                          layer_names.get_layer_idx(pf.target_layer_),
                          pf.zoom_levels_, std::move(pf.metadata_),
                          std::move(*pf.geometry_)});
}

void feature_handler::node(osmium::Node const& n) {
  handle_feature(inserter_, layer_names_builder_, shared_metadata_builder_,
                 runner_->process_node_, n);
}
void feature_handler::way(osmium::Way const& w) {
  handle_feature(inserter_, layer_names_builder_, shared_metadata_builder_,
                 runner_->process_way_, w);
}
void feature_handler::area(osmium::Area const& a) {
  handle_feature(inserter_, layer_names_builder_, shared_metadata_builder_,
                 runner_->process_area_, a);
}

}  // namespace tiles
