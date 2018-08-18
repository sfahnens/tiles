#pragma once

#include <map>
#include <string>
#include <utility>

#include "protozero/types.hpp"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

constexpr uint32_t kInvalidZoomLevel = 0x7F;  // 127; max for one byte in varint
constexpr fixed_coord_t kInvalidBoxHint =
    std::numeric_limits<fixed_coord_t>::max();

struct feature {
  uint64_t id_;
  std::pair<uint32_t, uint32_t> zoom_levels_;
  std::map<std::string, std::string> meta_;
  fixed_geometry geometry_;
};

namespace tags {

enum class Feature : protozero::pbf_tag_type {
  required_uint32_minzoomlevel = 1,
  required_uint32_maxzoomlevel = 2,

  required_uint64_box_x = 3,
  required_uint64_box_y = 4,

  required_uint64_id = 5,
  repeated_string_keys = 6,
  repeated_string_values = 7,

  repeated_string_simplify_masks = 8,
  required_FixedGeometry_geometry = 9
};

}  // namespace tags
}  // namespace tiles
