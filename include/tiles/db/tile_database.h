#pragma once

#include <map>

#include "lmdb/lmdb.hpp"

namespace tiles {

struct tile_database {
  tile_database() : env_{} {
    env_.set_mapsize(1024 * 1024 * 1024);
    env_.open("./");
  }


  // (x, y) -> idx
  std::map<std::pair<uint32_t, uint32_t>, size_t> fill_state_;

  lmdb::env env_;
};

}  // namespace tiles
