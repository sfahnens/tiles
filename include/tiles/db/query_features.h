#pragma once

#include "geo/tile.h"

#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"

namespace tiles {

geo::tile_range get_feature_range(lmdb::cursor& c) {
  auto minx = std::numeric_limits<uint32_t>::max();
  auto miny = std::numeric_limits<uint32_t>::max();
  auto maxx = std::numeric_limits<uint32_t>::min();
  auto maxy = std::numeric_limits<uint32_t>::min();

  constexpr uint32_t z = 10;
  for (auto el = c.get<tile_index_t>(lmdb::cursor_op::FIRST); el;
       el = c.get<tile_index_t>(lmdb::cursor_op::NEXT)) {
    auto const tile = feature_key_to_tile(el->first, z);
    minx = std::min(minx, tile.x_);
    miny = std::min(miny, tile.y_);
    maxx = std::max(maxx, tile.x_);
    maxy = std::max(maxy, tile.y_);
  }
  return geo::make_tile_range(minx, miny, maxx, maxy, z);
}

template <typename Fn>
void query_features(lmdb::cursor& c, geo::tile const& tile, Fn&& fn) {
  constexpr uint32_t z = 10;

  auto const bounds = tile.bounds_on_z(z);  // maybe some more indices :)
  for (auto y = bounds.miny_; y < bounds.maxy_; ++y) {
    auto const key_begin = make_feature_key(bounds.minx_, y, z);
    auto const key_end = make_feature_key(bounds.maxx_, y, z);

    for (auto el = c.get(lmdb::cursor_op::SET_RANGE, key_begin);
         el && el->first < key_end;
         el = c.get<decltype(key_begin)>(lmdb::cursor_op::NEXT)) {
      fn(el->second);
    }
  }
}

template <typename Fn>
void query_features(lmdb::env& db_env, geo::tile const& tile,
                    char const* dbi_name, Fn&& fn) {
  auto txn = lmdb::txn{db_env};
  auto dbi = txn.dbi_open(dbi_name, lmdb::dbi_flags::INTEGERKEY);
  auto c = lmdb::cursor{txn, dbi};

  query_features(c, tile, fn);
}

}  // namespace tiles
