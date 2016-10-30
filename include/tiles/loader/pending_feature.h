#pragma once

#include "osmium/osm.hpp"

#include "tiles/globals.h"

namespace tiles {

struct pending_feature {
  pending_feature(osmium::OSMObject const& obj) : obj_(obj), is_approved_() {}

  int64_t get_id() {
    return obj_.id();
  }

  bool has_tag(std::string const& key, std::string const& value) {
    // call .tags() and cache the result
    return value == obj_.get_value_by_key(key.c_str(), "");
  }

  // void set_approved(bool value = true) { is_approved_ = value; }

  void set_approved(size_t min = 0, size_t max = (kMaxZoomLevel + 1)) {
    // bounds checking!
    for (auto i = min; i <= max; ++i) {
      is_approved_[i] = true;
    }
  }

  void set_target_layer(std::string target_layer) {
    target_layer_ = std::move(target_layer);
  }

  osmium::OSMObject const& obj_;

  std::array<bool, kMaxZoomLevel + 1> is_approved_;
  std::string target_layer_;
};

struct pending_node : public pending_feature {
  pending_node(osmium::Node const& node) : pending_feature(node) {}
};

struct pending_way {};
struct pending_relation {};

}  // namespace tiles