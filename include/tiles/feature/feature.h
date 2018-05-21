#pragma once

#include <map>
#include <string>
#include <utility>

#include "protozero/types.hpp"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

constexpr uint32_t kInvalidZoomLevel = 0x7F;  // 127; max for one byte in varint

struct feature {
  bool is_valid() const {
    return zoom_levels_.first != kInvalidZoomLevel &&
           zoom_levels_.second != kInvalidZoomLevel;
  }

  uint64_t id_;
  std::pair<uint32_t, uint32_t> zoom_levels_;
  std::map<std::string, std::string> meta_;
  fixed_geometry geometry_;
};

namespace tags {

enum class Feature : protozero::pbf_tag_type {
  required_uint32_minzoomlevel = 1,
  required_uint32_maxzoomlevel = 2,
  required_uint64_id = 3,
  repeated_string_keys = 4,
  repeated_string_values = 5,
  required_FixedGeometry_geometry = 6
};

}  // namespace tags
}  // namespace tiles
