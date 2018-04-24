#pragma once

#include <map>

#include "lmdb/lmdb.hpp"

namespace tiles {

constexpr auto kDefaultFeatures = "default_features";
constexpr auto kDefaultTiles = "default_tiles";

inline lmdb::env make_tile_database(
    char const* path, lmdb::env_open_flags flags = lmdb::env_open_flags::NONE) {
  lmdb::env e;
  e.set_mapsize(1024 * 1024 * 1024);
  e.set_maxdbs(8);
  e.open(path, flags);

  return e;
}

struct batch_inserter {
  batch_inserter(lmdb::env& env, char const* dbname,
                 lmdb::dbi_flags flags = lmdb::dbi_flags::CREATE)
      : txn_{env}, dbi_{txn_.dbi_open(dbname, flags)}, done_{false} {}

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
  feature_inserter(lmdb::env& env, char const* dbname, lmdb::dbi_flags flags)
      : batch_inserter(env, dbname, flags) {}

  // (x, y) -> idx
  std::map<std::pair<uint32_t, uint32_t>, size_t> fill_state_;
};

}  // namespace tiles
