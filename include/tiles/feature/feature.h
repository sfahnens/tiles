#pragma once

#include <map>
#include <string>

#include "protozero/types.hpp"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

struct feature {
  std::map<std::string, std::string> meta_;
  fixed_geometry geometry_;
};

namespace tags {

enum class Feature : protozero::pbf_tag_type {
  repeated_string_keys = 1,
  repeated_string_values = 2,
  required_FixedGeometry_geometry = 3
};

} // namespace tags
} // namespace tiles
