#include "boost/filesystem.hpp"

#include "tiles/database.h"
#include "tiles/loader/loader.h"

using namespace tiles;

int main() {
  // loader l{"/data/osm/hessen-lates.osm.pbf"};
  // l.load();

  if (!boost::filesystem::is_directory(kDatabasePath)) {
    init_spatial_db(kDatabasePath);
  }

  load();
}
