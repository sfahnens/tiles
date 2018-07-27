#pragma once

#include <map>

#include "geo/tile.h"
#include "lmdb/lmdb.hpp"

#include "tiles/db/feature_pack.h"
#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/feature/feature.h"
#include "tiles/feature/serialize.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/util.h"

namespace tiles {

struct feature_inserter : public batch_inserter {
  feature_inserter(
      tile_db_handle& handle,
      lmdb::txn::dbi (tile_db_handle::*dbi_opener)(lmdb::txn&, lmdb::dbi_flags),
      lmdb::dbi_flags flags = lmdb::dbi_flags::CREATE)
      : batch_inserter(handle, dbi_opener, flags) {
    init_fill_state();
  }

  feature_inserter(lmdb::env& env, char const* dbname, lmdb::dbi_flags flags)
      : batch_inserter(env, dbname, flags) {
    init_fill_state();
  }

  ~feature_inserter() {
    if (!txn_.committed_ && !done_) {
      flush();
    }
  }

  void insert(feature const& f) {
    auto const box = bounding_box(f.geometry_);
    auto const value = serialize_feature(f);

    for (auto const& tile : make_tile_range(box)) {
      auto& pending = pending_features_[tile];
      pending.push_back(value);
      if (pending.size() > 100000) {
        persist(tile, pending);
        auto it = pending_features_.find(tile);
        verify(it != end(pending_features_), "cannot happen");
        pending_features_.erase(it);
      }
    }
  }

  void insert_unbuffered(geo::tile const& tile, std::string const& str) {
    persist(tile, {str});
  }

  void flush() {
    for (auto const & [ tile, features ] : pending_features_) {
      persist(tile, features);
    }
    pending_features_ = {};
  }

  void persist(geo::tile const& tile,
               std::vector<std::string> const& features) {
    auto const idx = ++fill_state_[tile];
    auto const key = make_feature_key(tile, idx);
    batch_inserter::insert(key, pack_features(features));
  }

private:
  void init_fill_state() {
    auto c = lmdb::cursor{txn_, dbi_};
    for (auto el = c.get<tile_index_t>(lmdb::cursor_op::FIRST); el;
         el = c.get<tile_index_t>(lmdb::cursor_op::NEXT)) {
      auto const tile = feature_key_to_tile(el->first);
      fill_state_[tile] = feature_key_to_idx(el->first);
    }
  }

  // (x, y) -> idx
  std::map<geo::tile, size_t> fill_state_;

  // (x, y) -> feature string
  std::map<geo::tile, std::vector<std::string>> pending_features_;
};

}  // namespace tiles
