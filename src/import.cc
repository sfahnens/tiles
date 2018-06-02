#include <iostream>

#include "conf/options_parser.h"
#include "conf/simple_config.h"

#include "tiles/db/clear_database.h"
#include "tiles/db/database_stats.h"
#include "tiles/db/prepare_tiles.h"
#include "tiles/db/tile_database.h"
#include "tiles/osm/load_coastlines.h"
#include "tiles/osm/load_osm.h"

namespace tiles {

// "/home/sebastian/Downloads/land-polygons-complete-4326.zip"
// "/data/osm/2017-10-29/hessen-171029.osm.pbf"

struct import_settings : public conf::simple_config {
  explicit import_settings(
      std::string const& db_fname = "tiles.mdb",
      std::string const& osm_fname = "latest.osm.pbf",
      std::string const& coastlines_fname = "coastlines.zip",
      std::vector<std::string> const& tasks = {"all"})
      : simple_config("tiles-import options", "") {
    string_param(db_fname_, db_fname, "db_fname", "/path/to/tiles.mdb");
    string_param(osm_fname_, osm_fname, "osm_fname", "/path/to/latest.osm.pbf");
    string_param(coastlines_fname_, coastlines_fname, "coastlines_fname",
                 "/path/to/coastlines.zip");
    multitoken_param(tasks_, tasks, "tasks",
                     "'all' or any combination of: 'coastlines', "
                     "'features', 'stats', 'tiles'");
  }

  bool has_any_task(std::vector<std::string> const& query) {
    return std::find(begin(tasks_), end(tasks_), "all") != end(tasks_) ||
           std::any_of(begin(query), end(query), [this](auto const& q) {
             return std::find(begin(tasks_), end(tasks_), q) != end(tasks_);
           });
  }

  std::string db_fname_;
  std::string osm_fname_;
  std::string coastlines_fname_;

  std::vector<std::string> tasks_;
};

}  // namespace tiles

int main(int argc, char** argv) {
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
    std::cout << "|> clear database\n";
    tiles::clear_database(opt.db_fname_);
  }

  lmdb::env db_env = tiles::make_tile_database(opt.db_fname_.c_str());
  tiles::tile_db_handle handle{db_env};

  if (opt.has_any_task({"coastlines"})) {
    tiles::scoped_timer t{"load coastlines"};
    tiles::load_coastlines(handle, opt.coastlines_fname_);
    std::cout << "|> sync db\n";
    db_env.sync();
  }

  if (opt.has_any_task({"features"})) {
    std::cout << "|> load features\n";
    tiles::load_osm(handle, opt.osm_fname_);
    std::cout << "|> sync db\n";
    db_env.sync();
  }

  if (opt.has_any_task({"stats"})) {
    tiles::database_stats(handle);
  }

  if (opt.has_any_task({"tiles"})) {
    std::cout << "|> prepare tiles\n";
    tiles::prepare_tiles(handle, 5);
  }

  std::cout << "|> import done!\n";
}
