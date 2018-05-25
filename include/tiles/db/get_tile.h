#pragma once

#include "geo/tile.h"
#include "lmdb/lmdb.hpp"

#include "tiles/db/render_tile.h"
#include "tiles/db/tile_index.h"

namespace tiles {

std::string get_tile(tile_db_handle& handle, geo::tile const& t) {
  auto txn = lmdb::txn{handle.env_};

  auto meta_dbi = handle.meta_dbi(txn);
  auto const max_prepared = txn.get(meta_dbi, kMetaKeyMaxPreparedZoomLevel);

  if (max_prepared &&
      static_cast<int>(t.z_) <= std::stoi(std::string{*max_prepared})) {
    auto tiles_dbi = handle.tiles_dbi(txn);
    auto db_tile = txn.get(tiles_dbi, make_tile_key(t));
    if (!db_tile) {
      return "";
    }

    std::cout << "found prepared tile" << std::endl;
    return std::string{*db_tile};
  }

  auto features_dbi = handle.features_dbi(txn);
  auto c = lmdb::cursor{txn, features_dbi};
  return compress_gzip(render_tile(c, t));
}

}  // namespace tiles
