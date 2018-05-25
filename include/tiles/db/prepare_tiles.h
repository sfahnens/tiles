#pragma once

#include <chrono>
#include <iomanip>

#include "geo/tile.h"

#include "tiles/db/render_tile.h"
#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/util.h"

namespace tiles {

struct prepare_stats {
  void update(geo::tile const& t) {
    if (prev_z_ == t.z_) {
      ++render_total_;
      return;
    }

    print_info();

    prev_z_ = t.z_;
    render_total_ = 1;
    render_empty_ = 0;
    tile_sizes_.clear();

    using namespace std::chrono;
    start_ = steady_clock::now();
  }

  void print_info() const {
    using namespace std::chrono;
    auto const now = steady_clock::now();

    std::cout << "rendered level " << std::setw(2) << prev_z_
              << " | total: " << std::setw(4) << render_total_
              << " empty: " << std::setw(4) << render_empty_ << " | ";

    double dur = duration_cast<microseconds>(now - start_).count() / 1000.0;
    if (dur < 1000) {
      std::cout << std::setw(6) << std::setprecision(4) << dur << "ms ";
    } else {
      dur /= 1000;
      std::cout << std::setw(6) << std::setprecision(4) << dur << "s  ";
    }

    size_t raw_sum = 0;
    size_t gzip_sum = 0;
    for (auto const & [ raw_size, gzip_size ] : tile_sizes_) {
      raw_sum += raw_size;
      gzip_sum += gzip_size;
    }
    std::cout << "| avg. raw: " << std::setw(4)
              << (raw_sum / tile_sizes_.size() / 1024) << "KB "
              << " avg. gzip: " << std::setw(4)
              << (gzip_sum / tile_sizes_.size() / 1024) << "KB\n";
  }

  void register_tile_size(size_t raw, size_t gzip) {
    tile_sizes_.emplace_back(raw, gzip);
  }

  uint32_t prev_z_ = 0;
  size_t render_total_ = 0;
  size_t render_empty_ = 0;
  std::chrono::time_point<std::chrono::steady_clock> start_ =
      std::chrono::steady_clock::now();

  std::vector<std::pair<size_t, size_t>> tile_sizes_;
};

void prepare_tiles(tile_db_handle& handle, uint32_t max_zoomlevel) {
  batch_inserter inserter{handle, &tile_db_handle::tiles_dbi};

  auto feature_dbi = handle.features_dbi(inserter.txn_);
  auto c = lmdb::cursor{inserter.txn_, feature_dbi};

  prepare_stats stats;

  for (auto const& tile : geo::make_tile_pyramid()) {
    stats.update(tile);

    if (tile.z_ > max_zoomlevel) {
      break;
    }

    auto const rendered_tile = render_tile(c, tile);
    if (rendered_tile.empty()) {
      ++stats.render_empty_;
      continue;
    }

    inserter.insert(make_tile_key(tile), rendered_tile);
  }

  auto meta_dbi = handle.meta_dbi(inserter.txn_);
  inserter.txn_.put(meta_dbi, kMetaKeyMaxPreparedZoomLevel,
                    std::to_string(max_zoomlevel));
}

void prepare_tiles_sparse(tile_db_handle& handle, uint32_t max_zoomlevel) {
  batch_inserter inserter{handle, &tile_db_handle::tiles_dbi};

  auto feature_dbi = handle.features_dbi(inserter.txn_);
  auto c = lmdb::cursor{inserter.txn_, feature_dbi};

  prepare_stats stats;
  auto const base_range = get_feature_range(c);
  for (auto z = 0u; z <= max_zoomlevel; ++z) {
    for (auto const& tile : geo::tile_range_on_z(base_range, z)) {
      stats.update(tile);

      auto const rendered_tile = render_tile(c, tile);
      if (rendered_tile.empty()) {
        ++stats.render_empty_;
        continue;
      }

      auto compressed_tile = compress_gzip(rendered_tile);
      stats.register_tile_size(rendered_tile.size(), compressed_tile.size());

      inserter.insert(make_tile_key(tile), compressed_tile);
    }
  }

  auto meta_dbi = handle.meta_dbi(inserter.txn_, lmdb::dbi_flags::CREATE);
  inserter.txn_.put(meta_dbi, kMetaKeyMaxPreparedZoomLevel,
                    std::to_string(max_zoomlevel));

  stats.print_info();
}

}  // namespace tiles
