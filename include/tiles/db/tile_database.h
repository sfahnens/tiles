#pragma once

#include <map>

#include "lmdb/lmdb.hpp"

namespace tiles {

constexpr auto kDefaultTiles = "default_tiles";

struct tile_database {
  tile_database() : env_{} {
    env_.set_mapsize(1024 * 1024 * 1024);
    env_.set_maxdbs(8);
    env_.open("./");
  }

  lmdb::env env_;
};

struct batch_inserter {
  batch_inserter(lmdb::env& env, char const* dbname)
      : txn_{env},
        dbi_{txn_.dbi_open(dbname, lmdb::dbi_flags::CREATE)},
        done_{false} {}

  ~batch_inserter() {
    if (!txn_.committed_ && !done_) {
      done_ = true;
      txn_.commit();
    }
  }

  template <typename... T>
  void insert(T&&... t) {
    txn_.put(dbi_, std::forward<T>(t)...);
  }

  lmdb::txn txn_;
  lmdb::txn::dbi dbi_;
  bool done_;
};

struct feature_inserter : public batch_inserter {
  feature_inserter(lmdb::env& env, char const* dbname)
      : batch_inserter(env, dbname) {}

  // (x, y) -> idx
  std::map<std::pair<uint32_t, uint32_t>, size_t> fill_state_;
};

}  // namespace tiles
