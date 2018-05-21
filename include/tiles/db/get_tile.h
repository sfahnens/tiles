#pragma once

#include "geo/tile.h"
#include "lmdb/lmdb.hpp"

#include "tiles/db/render_tile.h"
#include "tiles/db/tile_index.h"

namespace tiles {

std::string get_tile(tile_db_handle& handle, geo::tile const& t) {
  auto txn = lmdb::txn{handle.env_};

  auto tiles_dbi = handle.tiles_dbi(txn);
  auto db_tile = txn.get(tiles_dbi, make_tile_key(t));
  if (db_tile) {
    std::cout << "found prepared tile" << std::endl;
    return std::string{*db_tile};
  }

  auto features_dbi = handle.features_dbi(txn);
  auto c = lmdb::cursor{txn, features_dbi};
  return render_tile(c, t);
}

}  // namespace tiles
