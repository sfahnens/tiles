#pragma once

#include <vector>

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

void load_shapefile(std::string const& fname,
                    std::function<void(fixed_simple_polygon)> const& consumer);

}  // namespace tiles
