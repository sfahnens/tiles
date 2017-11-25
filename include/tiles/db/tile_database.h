#pragma once

#include "lmdb/lmdb.hpp"

namespace tiles {

struct tile_database {
  tile_database() : env_{} {
    env_.set_mapsize(1024 * 1024);
    env_.open("./");
  }

  lmdb::env env_;
};

}  // namespace tiles
