#pragma once

#include <vector>

#include "rocksdb/slice.h"

#include "tiles/mvt/tags.h"
#include "tiles/mvt/tile_spec.h"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

void encode_geometry(protozero::pbf_builder<tags::Feature>&,
                     fixed_geometry const&, tile_spec const&);

}  // namespace tiles
