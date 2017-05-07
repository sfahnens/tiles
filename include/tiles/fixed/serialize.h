#pragma once

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

std::string serialize(fixed_xy const&);
std::string serialize(fixed_polyline const&);

}  // namespace tiles
