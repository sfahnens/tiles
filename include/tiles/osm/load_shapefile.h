#pragma once

#include<vector>

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

std::vector<fixed_geometry> load_shapefile(std::string const& fname);

}  // namespace tiles
