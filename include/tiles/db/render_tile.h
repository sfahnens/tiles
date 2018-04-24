#pragma once

#include "tiles/db/query_features.h"
#include "tiles/db/tile_database.h"
#include "tiles/feature/deserialize.h"
#include "tiles/mvt/tile_builder.h"

namespace tiles {

std::string render_tile(lmdb::cursor& c, geo::tile const& tile) {
  tile_builder builder{tile};
  query_features(c, tile, [&](auto const& str) {
    auto const feature = deserialize_feature(str, tile.z_);
    if (!feature.is_valid()) {
      return;
    }
    builder.add_feature(feature);
  });
  return builder.finish();
}

std::string render_tile(lmdb::env& db_env, geo::tile const& tile,
                        char const* dbi_name = kDefaultFeatures) {
  tile_builder builder{tile};
  query_features(db_env, tile, dbi_name, [&](auto const& str) {
    auto const feature = deserialize_feature(str, tile.z_);
    if (!feature.is_valid()) {
      return;
    }
    builder.add_feature(feature);
  });
  return builder.finish();
}

}  // namespace tiles
