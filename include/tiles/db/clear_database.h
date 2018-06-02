#pragma once

#include "tiles/db/tile_database.h"


namespace tiles {

void clear_database(std::string const& db_fname) {
  lmdb::env db_env = make_tile_database(db_fname.c_str());
  tile_db_handle handle{db_env};

  lmdb::txn txn{handle.env_};
  auto meta_dbi = handle.meta_dbi(txn, lmdb::dbi_flags::CREATE);
  meta_dbi.clear();

  auto features_dbi = handle.features_dbi(txn, lmdb::dbi_flags::CREATE);
  features_dbi.clear();

  auto tiles_dbi = handle.tiles_dbi(txn, lmdb::dbi_flags::CREATE);
  tiles_dbi.clear();

  txn.commit();
}

}  // namespace tiles
