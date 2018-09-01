#pragma once

#include <map>
#include <string>
#include <utility>

#include "protozero/types.hpp"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

constexpr uint32_t kInvalidZoomLevel = 0x3F;  // 63; max for one byte in svarint
constexpr fixed_coord_t kInvalidBoxHint =
    std::numeric_limits<fixed_coord_t>::max();

struct feature {
  uint64_t id_;
  size_t layer_;
  std::pair<uint32_t, uint32_t> zoom_levels_;
  std::map<std::string, std::string> meta_;
  fixed_geometry geometry_;
};

namespace tags {

enum class Feature : protozero::pbf_tag_type {
  packed_sint64_header = 1,

  required_uint64_id = 2,
  repeated_string_keys = 3,
  repeated_string_values = 4,

  repeated_string_simplify_masks = 5,
  required_FixedGeometry_geometry = 6
};

}  // namespace tags
}  // namespace tiles
