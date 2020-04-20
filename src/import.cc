#include <iostream>

#include "conf/configuration.h"
#include "conf/options_parser.h"

#include "tiles/db/clear_database.h"
#include "tiles/db/database_stats.h"
#include "tiles/db/feature_inserter_mt.h"
#include "tiles/db/feature_pack.h"
#include "tiles/db/pack_file.h"
#include "tiles/db/prepare_tiles.h"
#include "tiles/db/tile_database.h"
#include "tiles/osm/load_coastlines.h"
#include "tiles/osm/load_osm.h"

namespace tiles {

struct import_settings : public conf::configuration {
  import_settings() : conf::configuration("tiles-import options", "") {
    param(db_fname_, "db_fname", "/path/to/tiles.mdb");
    param(osm_fname_, "osm_fname", "/path/to/latest.osm.pbf");
    param(osm_profile_, "osm_profile", "/path/to/profile.lua");
    param(coastlines_fname_, "coastlines_fname", "/path/to/coastlines.zip");
    param(tasks_, "tasks",
          "'all' or any combination of: 'coastlines', "
          "'features', 'stats', 'pack', 'tiles'");
  }

  bool has_any_task(std::vector<std::string> const& query) const {
    return std::find(begin(tasks_), end(tasks_), "all") != end(tasks_) ||
           std::any_of(begin(query), end(query), [this](auto const& q) {
             return std::find(begin(tasks_), end(tasks_), q) != end(tasks_);
           });
  }

  std::string db_fname_{"tiles.mdb"};
  std::string osm_fname_{"planet-latest.osm.pbf"};
  std::string osm_profile_{"../profile/profile.lua"};
  std::string coastlines_fname_{"land-polygons-complete-4326.zip"};
  std::vector<std::string> tasks_{{"all"}};
};

}  // namespace tiles

int main(int argc, char const** argv) {
  tiles::import_settings opt;

  try {
    conf::options_parser parser({&opt});
    parser.read_command_line_args(argc, argv, false);

    if (parser.help() || parser.version()) {
      std::cout << "tiles-import\n\n";
      parser.print_help(std::cout);
      return 0;
    }

    parser.read_configuration_file(false);
    parser.print_used(std::cout);
  } catch (std::exception const& e) {
    std::cout << "options error: " << e.what() << "\n";
    return 1;
  }

  if (opt.has_any_task({"coastlines", "features"})) {
    tiles::t_log("clear database");
    tiles::clear_database(opt.db_fname_);
    tiles::clear_pack_file(opt.db_fname_.c_str());
  }

  lmdb::env db_env = tiles::make_tile_database(opt.db_fname_.c_str());
  tiles::tile_db_handle db_handle{db_env};
  tiles::pack_handle pack_handle{opt.db_fname_.c_str()};

  {
    tiles::feature_inserter_mt inserter{
        tiles::dbi_handle{db_handle, db_handle.features_dbi_opener()},
        pack_handle};

    if (opt.has_any_task({"coastlines"})) {
      tiles::scoped_timer t{"load coastlines"};
      tiles::load_coastlines(db_handle, inserter, opt.coastlines_fname_);
    }

    if (opt.has_any_task({"features"})) {
      tiles::t_log("load features");
      tiles::load_osm(db_handle, inserter, opt.osm_fname_, opt.osm_profile_);
    }
  }

  if (opt.has_any_task({"stats"})) {
    tiles::database_stats(db_handle, pack_handle);
  }

  if (opt.has_any_task({"pack"})) {
    tiles::t_log("pack features");
    tiles::pack_features(db_handle, pack_handle);
  }

  if (opt.has_any_task({"tiles"})) {
    tiles::t_log("prepare tiles");
    tiles::prepare_tiles(db_handle, pack_handle, 10);
  }

  tiles::t_log("import done!");
}
