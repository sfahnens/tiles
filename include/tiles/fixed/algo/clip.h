#pragma once

#include "tiles/fixed/fixed_geometry.h"

#include "tiles/tile_spec.h"

namespace tiles {

fixed_geometry clip(fixed_geometry const&, tile_spec const&);

}  // namespace tiles
