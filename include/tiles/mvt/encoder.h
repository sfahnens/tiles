#pragma once

#include <vector>

#include "rocksdb/slice.h"

#include "tiles/mvt/tags.h"
#include "tiles/mvt/tile_spec.h"

namespace tiles {

std::pair<tags::GeomType, std::vector<uint32_t>> encode_geometry(
    rocksdb::Slice const&, tile_spec const&);

}  // namespace tiles
