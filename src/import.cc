#include <iostream>

#include "tiles/fixed/fixed_geometry.h"

// #include "boost/filesystem.hpp"

// #include "tiles/database.h"
// #include "tiles/loader/loader.h"
#include "tiles/loader/loader_2.h"

using namespace tiles;

int main() {
  // loader l{"/data/osm/hessen-lates.osm.pbf"};
  // l.load();

  // if (!boost::filesystem::is_directory(kDatabasePath)) {
  //   init_spatial_db(kDatabasePath);
  // }

  load_2();

  std::cout << "exit" << std::endl;
}
