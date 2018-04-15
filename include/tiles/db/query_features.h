#pragma once

#include "geo/tile.h"

#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"

namespace tiles {

template <typename Fn>
void query_features(tile_database& db, geo::tile const& tile, Fn&& fn) {
  auto txn = lmdb::txn{db.env_};
  auto dbi = txn.dbi_open();
  auto c = lmdb::cursor{txn, dbi};

  constexpr uint32_t z = 10;

  auto const bounds = tile.bounds_on_z(z);  // maybe some more indices :)
  for (auto y = bounds.miny_; y < bounds.maxy_; ++y) {
    auto const key_begin = std::to_string(make_feature_key(bounds.minx_, y, z));
    auto const key_end = std::to_string(make_feature_key(bounds.maxx_, y, z));

    for (auto el = c.get(lmdb::cursor_op::SET_RANGE, key_begin);
         el && el->first < key_end; el = c.get(lmdb::cursor_op::NEXT)) {
      fn(el->second);
    }
  }
}

}  // namespace tiles
