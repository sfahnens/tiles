#pragma once

#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/feature/feature.h"
#include "tiles/feature/serialize.h"
#include "tiles/fixed/algo/bounding_box.h"

namespace tiles {

inline void insert_feature(feature_inserter& inserter, feature const& f) {
  auto const box = bounding_box(f.geometry_);
  auto const value = serialize_feature(f);

  uint32_t z = 10;  // whatever
  for (auto const& tile : make_tile_range(box, z)) {
    auto const idx = inserter.fill_state_[{tile.x_, tile.y_}]++;

    auto key = make_feature_key(tile, idx);
    inserter.insert(key, value);
  }
}

}  // namespace tiles
