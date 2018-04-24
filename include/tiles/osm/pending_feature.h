#pragma once

#include <vector>

#include "osmium/osm.hpp"

#include "sol.hpp"

namespace tiles {

struct pending_feature {
  pending_feature(osmium::OSMObject const& obj)
      : obj_(obj), is_approved_(false) {}

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
    if(std::distance(va.begin(), va.end()) == 0) {
      return obj_.get_value_by_key(key.c_str()) != nullptr;
    } else {
      auto const actual_value = obj_.get_value_by_key(key.c_str(), "");
      return std::any_of(std::begin(va), std::end(va),
                         [&actual_value](std::string const& value) {
                           return actual_value == value;
                         });
    }
  }

  void set_approved_min(uint32_t min) {
    set_approved(min, (kMaxZoomLevel + 1));
  }

  void set_approved_full() {
    set_approved(0, (kMaxZoomLevel + 1));
  }

  // default parameters do not work with lua stuff
  void set_approved(uint32_t min = 0, uint32_t max = (kMaxZoomLevel + 1)) {
    is_approved_ = true;
    zoom_levels_ = {min, max};
  }

  void set_target_layer(std::string target_layer) {
    target_layer_ = std::move(target_layer);
  }

  void add_tag_as_metadata(std::string tag) {
    tag_as_metadata_.emplace_back(tag);
  }

  osmium::OSMObject const& obj_;

  bool is_approved_;
  std::pair<uint32_t, uint32_t> zoom_levels_;

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