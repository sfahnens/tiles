#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <random>

#include "conf/configuration.h"
#include "conf/options_parser.h"

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "tiles/db/tile_database.h"
#include "tiles/get_tile.h"
#include "tiles/perf_counter.h"

namespace tiles {

struct benchmark_settings : public conf::configuration {
  benchmark_settings() : configuration("tiles-benchmark options", "") {
    param(db_fname_, "db_fname", "/path/to/tiles.mdb");
    param(tile_, "tile", "xyz coords of a tile, if not present random smaple");
    param(compress_, "compress", "compress the tiles");
  }

  std::string db_fname_{"tiles.mdb"};
  std::vector<uint32_t> tile_;
  bool compress_{true};
};

}  // namespace tiles

int main(int argc, char const** argv) {
  tiles::benchmark_settings opt;

  try {
    conf::options_parser parser({&opt});
    parser.read_command_line_args(argc, argv, false);

    if (parser.help() || parser.version()) {
      std::cout << "tiles-benchmark\n\n";
      parser.print_help(std::cout);
      return 0;
    }

    parser.read_configuration_file(false);
    parser.print_used(std::cout);
  } catch (std::exception const& e) {
    std::cout << "options error: " << e.what() << "\n";
    return 1;
  }

  lmdb::env db_env = tiles::make_tile_database(opt.db_fname_.c_str());
  tiles::tile_db_handle db_handle{db_env};
  tiles::pack_handle pack_handle{opt.db_fname_.c_str()};

  auto render_ctx = make_render_ctx(db_handle);
  render_ctx.ignore_prepared_ = true;
  render_ctx.compress_result_ = opt.compress_;

  if (opt.tile_.empty()) {
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

      auto txn = db_handle.make_txn();
      auto features_dbi = db_handle.features_dbi(txn);
      auto features_cursor = lmdb::cursor{txn, features_dbi};

      tiles::perf_counter pc;
      for (auto const& tile : geo::make_tile_range(p1, p2, z)) {
        auto rendered_tile = tiles::get_tile(db_handle, txn, features_cursor,
                                             pack_handle, render_ctx, tile, pc);
        // break;
      }
      tiles::perf_report_get_tile(pc);
    }

  } else {
    utl::verify(opt.tile_.size() == 3, "need exactly three coordinats: x y z");
    geo::tile tile{opt.tile_[0], opt.tile_[1], opt.tile_[2]};
    std::cout << "render tile: " << tile << std::endl;

    auto txn = db_handle.make_txn();
    auto features_dbi = db_handle.features_dbi(txn);
    auto features_cursor = lmdb::cursor{txn, features_dbi};

    render_ctx.tb_aggregate_lines_ = true;
    render_ctx.tb_aggregate_polygons_ = true;
    render_ctx.tb_print_stats_ = true;

    tiles::perf_counter pc;
    auto const rendered_tile = tiles::get_tile(
        db_handle, txn, features_cursor, pack_handle, render_ctx, tile, pc);
    tiles::perf_report_get_tile(pc);
  }
}
