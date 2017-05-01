#include "tiles/loader/script_runner.h"

#include "sol.hpp"

namespace tiles {

struct script_runner::impl {
  void initialize() {
    lua_.script_file("../profile/profile.lua");
    lua_.open_libraries(sol::lib::base, sol::lib::package);

    lua_.new_usertype<pending_feature>(  //
        "pending_feature",  //
        "get_id", &pending_feature::get_id,  //
        "has_tag", &pending_feature::has_tag,  //
        "has_any_tag", &pending_feature::has_any_tag,  //
        "set_approved", &pending_feature::set_approved,  //
        "set_target_layer", &pending_feature::set_target_layer,  //
        "add_tag_as_metadata", &pending_feature::add_tag_as_metadata);

    lua_.new_usertype<pending_node>("pending_node", sol::base_classes,
                                    sol::bases<pending_feature>());

    lua_.new_usertype<pending_way>("pending_way", sol::base_classes,
                                   sol::bases<pending_feature>());

    process_node_ = lua_["process_node"];
    process_way_ = lua_["process_way"];
    process_relation_ = lua_["process_relation"];
  }

  sol::state lua_;

  sol::function process_node_;
  sol::function process_way_;
  sol::function process_relation_;
};

script_runner::script_runner()
    : impl_(std::make_unique<script_runner::impl>()) {
  impl_->initialize();
}
script_runner::~script_runner() = default;

void script_runner::process_node(pending_node& node) {
  impl_->process_node_(node);
}

void script_runner::process_way(pending_way& way) { impl_->process_way_(way); }

void script_runner::process_relation(pending_relation& relation) {
  impl_->process_relation_(relation);
}

}  // namespace tiles