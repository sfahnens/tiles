#pragma once

#include <string>

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

fixed_geometry deserialize(std::string_view geo);
fixed_geometry deserialize(std::string_view geo,
                           std::vector<std::string_view> simplify_masks,
                           uint32_t z);

}  // namespace tiles
