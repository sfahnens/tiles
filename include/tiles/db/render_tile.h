#pragma once

#include "tiles/db/query_features.h"
#include "tiles/db/tile_database.h"
#include "tiles/feature/deserialize.h"
#include "tiles/mvt/tile_builder.h"

namespace tiles {

std::string render_tile(tile_database& db, geo::tile const& tile) {
  tile_builder builder{tile};
  query_features(db, tile, [&](auto const& str) {
    auto const feature = deserialize_feature(str);
    builder.add_feature(feature);
  });
  return builder.finish();
}

}  // namespace tiles
