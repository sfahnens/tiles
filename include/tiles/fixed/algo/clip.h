#pragma once

#include "tiles/fixed/fixed_geometry.h"
#include "tiles/tile_spec.h"

namespace tiles {

fixed_geometry clip(fixed_geometry const&, fixed_box const&);

// TODO this is just a quickfix
inline fixed_geometry clip(fixed_geometry const& geo, tile_spec const& spec) {
  fixed_box box{{spec.overdraw_bounds_.minx_, spec.overdraw_bounds_.miny_},
                {spec.overdraw_bounds_.maxx_, spec.overdraw_bounds_.maxy_}};
  return clip(geo, box);
}

}  // namespace tiles
