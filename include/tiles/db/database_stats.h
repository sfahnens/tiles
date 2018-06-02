#pragma once

#include "tiles/db/tile_index.h"

namespace tiles {

void database_stats(tile_db_handle& handle) {
  auto txn = lmdb::txn{handle.env_};
  auto dbi = txn.dbi_open(kDefaultFeatures, lmdb::dbi_flags::INTEGERKEY);
  auto c = lmdb::cursor{txn, dbi};

  size_t count = 0;
  for (auto el = c.get<tile_index_t>(lmdb::cursor_op::FIRST); el;
       el = c.get<tile_index_t>(lmdb::cursor_op::NEXT)) {
    ++count;
  }

  std::cout << count << " entries in db" << std::endl;
}

}  // namespace tiles
