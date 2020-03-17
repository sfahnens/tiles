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
#include "tiles/fixed/io/dump.h"
#include "tiles/util.h"

namespace tiles {

struct cache_bucket {
  geo::tile tile_;
  std::atomic_size_t fill_state_{0};  // nth entry in the database

  std::mutex mutex_;
  size_t mem_size_{0};
  std::vector<std::string> mem_;
};

struct feature_inserter_mt {
  static constexpr size_t kCacheThresholdUpper = 1024ull * 1024 * 1024;
  static constexpr size_t kCacheThresholdLower = kCacheThresholdUpper / 4 * 3;

  feature_inserter_mt(dbi_handle handle)
      : handle_{std::move(handle)},
        cache_((1 << kTileDefaultIndexZoomLvl) *
               (1 << kTileDefaultIndexZoomLvl)) {
    auto it = geo::tile_iterator{kTileDefaultIndexZoomLvl};
    for (auto i = 0ul; i < cache_.size(); ++i) {
      utl::verify(it->z_ == kTileDefaultIndexZoomLvl, "it broken");
      cache_[i].tile_ = *it;
      ++it;
    }
    utl::verify(it->z_ == kTileDefaultIndexZoomLvl + 1, "it broken");

    auto [txn, dbi] = handle_.begin_txn();
    auto c = lmdb::cursor{txn, dbi};
    for (auto el = c.get<tile_index_t>(lmdb::cursor_op::FIRST); el;
         el = c.get<tile_index_t>(lmdb::cursor_op::NEXT)) {
      auto const tile = feature_key_to_tile(el->first);
      get_bucket(tile).fill_state_ = feature_key_to_idx(el->first);
    }
  }

  ~feature_inserter_mt() { flush(0, 0); }

  void insert(feature const& f) {
    auto const box = bounding_box(f.geometry_);
    auto const value = serialize_feature(f);

    for (auto const& tile : make_tile_range(box)) {
      cache_size_ += value.size();
      auto& bucket = get_bucket(tile);

      std::lock_guard<std::mutex> l(bucket.mutex_);
      bucket.mem_size_ += value.size();
      bucket.mem_.push_back(value);
    }

    flush();
  }

  void flush(size_t threshold_upper = kCacheThresholdUpper,
             size_t threshold_lower = kCacheThresholdLower) {
    size_t persisted_packs = 0;
    size_t persisted_features = 0;
    size_t persisted_size = 0;

    int64_t min_x = std::numeric_limits<int64_t>::max();
    int64_t max_x = std::numeric_limits<int64_t>::min();
    int64_t min_y = std::numeric_limits<int64_t>::max();
    int64_t max_y = std::numeric_limits<int64_t>::min();

    std::vector<std::pair<cache_bucket*, std::vector<std::string>>> queue;
    {  // phase 1: build queue
      if (cache_size_ <= threshold_upper) {
        return;
      }
      std::lock_guard<std::mutex> flush_lock{flush_mutex_};
      if (cache_size_ <= threshold_upper) {
        return;
      }

      std::vector<std::pair<size_t, cache_bucket*>> buckets;
      buckets.reserve(cache_.size());
      for (auto& b : cache_) {
        if (b.mem_size_ > 0) {
          buckets.emplace_back(b.mem_size_, &b);
        }
      }
      std::sort(begin(buckets), end(buckets));
      if (buckets.empty()) {
        return;
      }

      for (auto const& [size, bucket_ptr] : buckets) {
        if (cache_size_ < threshold_lower) {
          break;
        }

        std::lock_guard<std::mutex> bucket_lock{bucket_ptr->mutex_};
        ++persisted_packs;
        persisted_features += bucket_ptr->mem_.size();

        min_x = std::min(min_x, static_cast<int64_t>(bucket_ptr->tile_.x_));
        max_x = std::max(max_x, static_cast<int64_t>(bucket_ptr->tile_.x_));
        min_y = std::min(min_y, static_cast<int64_t>(bucket_ptr->tile_.y_));
        max_y = std::max(max_y, static_cast<int64_t>(bucket_ptr->tile_.y_));

        cache_size_ -= bucket_ptr->mem_size_;
        persisted_size += bucket_ptr->mem_size_;
        bucket_ptr->mem_size_ = 0;

        queue.emplace_back(bucket_ptr, std::vector<std::string>{});
        std::swap(queue.back().second, bucket_ptr->mem_);
      }
    }
    {  // phase 2: write to database
      auto [txn, dbi] = handle_.begin_txn();
      for (auto const& [bucket_ptr, features] : queue) {
        txn.put(
            dbi,
            make_feature_key(bucket_ptr->tile_, ++(bucket_ptr->fill_state_)),
            pack_features(features));
      }
      txn.commit();
    }

    t_log("persisted {} packs with {} features ({}) bounds ({}, {} / {}, {})",
          printable_num{persisted_packs}, printable_num{persisted_features},
          printable_bytes{persisted_size}, min_x, min_y, max_x, max_y);
  }

  cache_bucket& get_bucket(geo::tile const tile) {
    auto it = std::lower_bound(
        begin(cache_), end(cache_), tile, [](auto const& a, auto const& b) {
          return std::tie(a.tile_.y_, a.tile_.x_) < std::tie(b.y_, b.x_);
        });

    utl::verify(it != end(cache_) && tile == it->tile_,
                "requested invalid tile");
    return *it;
  }

  dbi_handle handle_;

  std::mutex flush_mutex_;
  std::atomic_size_t cache_size_{0};
  std::vector<cache_bucket> cache_;
};

}  // namespace tiles
