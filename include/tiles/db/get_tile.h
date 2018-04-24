#pragma once

#include "geo/tile.h"
#include "lmdb/lmdb.hpp"

#include "tiles/db/render_tile.h"
#include "tiles/db/tile_index.h"

namespace tiles {

std::string get_tile(lmdb::env& db_env, geo::tile const& t,
                     char const* feature_dbi_name = kDefaultFeatures,
                     char const* tiles_dbi_name = kDefaultTiles) {
  auto txn = lmdb::txn{db_env};

  auto tiles_dbi = txn.dbi_open(tiles_dbi_name, lmdb::dbi_flags::INTEGERKEY);
  auto db_tile = txn.get(tiles_dbi, make_tile_key(t));
  if (db_tile) {
    std::cout << "found prepared tile" << std::endl;
    return std::string{*db_tile};
  }

  auto features_dbi =
      txn.dbi_open(feature_dbi_name, lmdb::dbi_flags::INTEGERKEY);
  auto c = lmdb::cursor{txn, features_dbi};
  return render_tile(c, t);
}

}  // namespace tiles
