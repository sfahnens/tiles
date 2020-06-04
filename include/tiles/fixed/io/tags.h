#pragma once

#include "protozero/types.hpp"

namespace tiles::tags {

enum fixed_geometry_type : int {
  UNKNOWN = 0,
  POINT = 1,
  POLYLINE = 2,
  POLYGON = 3
};

enum class fixed_geometry : protozero::pbf_tag_type {
  required_fixed_geometry_type = 1,
  packed_sint64_geometry = 2
};

}  // namespace tiles::tags
