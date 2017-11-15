#pragma once

#include <vector>

#include "osmium/osm.hpp"

#include "sol.hpp"

#include "tiles/tile_spec.h"

namespace tiles {

struct pending_feature {
  pending_feature(osmium::OSMObject const& obj) : obj_(obj), is_approved_() {}

  int64_t get_id() { return obj_.id(); }

  bool has_tag(std::string const& key, std::string const& value) {
    // TODO call .tags() and cache the result
    return value == obj_.get_value_by_key(key.c_str(), "");
  }

  bool has_any_tag(std::string const& key,
                   std::vector<std::string> const values) {
    auto const actual_value = obj_.get_value_by_key(key.c_str(), "");
    return std::any_of(
        begin(values), end(values),
        [&actual_value](auto const& value) { return actual_value == value; });
  }


    bool has_any_tag2(std::string const& key, sol::variadic_args va) {
    auto const actual_value = obj_.get_value_by_key(key.c_str(), "");
    return std::any_of(
        std::begin(va), std::end(va),
        [&actual_value](std::string const& value) { return actual_value == value; });
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

  void add_tag_as_metadata(std::string tag) {
    tag_as_metadata_.emplace_back(tag);
  }

  osmium::OSMObject const& obj_;

  std::array<bool, kMaxZoomLevel + 1> is_approved_;
  std::string target_layer_;

  std::vector<std::string> tag_as_metadata_;
};

struct pending_node : public pending_feature {
  pending_node(osmium::Node const& node) : pending_feature(node) {}
};

struct pending_way : public pending_feature {
  pending_way(osmium::Way const& way) : pending_feature(way) {}
};

struct pending_relation {};

}  // namespace tiles