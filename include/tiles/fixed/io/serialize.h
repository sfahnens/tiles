#pragma once

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

std::string serialize(fixed_point const&);
std::string serialize(fixed_polyline const&);
std::string serialize(fixed_polygon const&);
std::string serialize(fixed_geometry const&);

}  // namespace tiles
