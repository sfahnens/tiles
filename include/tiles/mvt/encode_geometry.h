#pragma once

#include "tiles/fixed/fixed_geometry.h"

#include "tiles/mvt/tags.h"
#include "tiles/mvt/tile_spec.h"

namespace tiles {

void encode_geometry(protozero::pbf_builder<tags::mvt::Feature>&,
                     fixed_geometry const&, tile_spec const&);

}  // namespace tiles
