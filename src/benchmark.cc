#include <algorithm>
#include <iostream>
#include <random>

#include "tiles/db/get_tile.h"
#include "tiles/db/tile_database.h"
#include "tiles/perf_counter.h"

#include "fmt/core.h"
#include "fmt/ostream.h"

int main() {
  lmdb::env db_env = tiles::make_tile_database("./tiles.mdb");
  tiles::tile_db_handle handle{db_env};
  auto render_ctx = make_render_ctx(handle);

  render_ctx.ignore_prepared_ = true;

  // geo::tile tile{536, 346, 10};  // TODO force reder
  // geo::tile tile{1073, 693, 11};

  geo::latlng p1{49.83, 8.55};
  geo::latlng p2{50.13, 8.74};

  for (auto z = 9; z < 18; z += 2) {

    std::vector<geo::tile> tiles;
    for (auto const& tile : geo::make_tile_range(p1, p2, z)) {
      tiles.push_back(tile);
    }
    std::mt19937 g(31337);
    std::shuffle(begin(tiles), end(tiles), g);

    fmt::print(std::cout, "=== process z {} ({} tiles)\n", z, tiles.size());

    lmdb::txn txn{handle.env_};
    auto features_dbi = handle.features_dbi(txn);
    auto features_cursor = lmdb::cursor{txn, features_dbi};

    size_t size_sum = 0;
    tiles::perf_counter pc;
    for (auto const& tile : geo::make_tile_range(p1, p2, z)) {
      auto rendered_tile =
          tiles::get_tile(handle, txn, features_cursor, render_ctx, tile, pc);
      size_sum += rendered_tile ? rendered_tile->size() : 0;
      // break;
    }
    tiles::perf_report_get_tile(pc);
    std::cout << "rendered tile: " << size_sum << " bytes" << std::endl;
  }
}
