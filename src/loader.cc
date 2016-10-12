#include <cstdlib>
#include <iostream>

#include "boost/filesystem.hpp"

#include "rocksdb/utilities/spatial_db.h"

#include "tiles/cities.h"
#include "tiles/flat_geometry.h"
#include "tiles/globals.h"
#include "tiles/slice.h"
#include "tiles/util.h"

using namespace rocksdb;
using namespace rocksdb::spatial;

using namespace tiles;
using namespace geo;

constexpr char kDatabasePath[] = "spatial";

void checked(Status&& status) {
  if (!status.ok()) {
    std::cout << "error: " << status.ToString() << std::endl;
    std::exit(1);
  }
}

int main() {
  if (!boost::filesystem::is_directory(kDatabasePath)) {
    std::cout << "creating database" << std::endl;
    checked(SpatialDB::Create(
        SpatialDBOptions(), kDatabasePath,
        {SpatialIndexOptions("zoom10", bbox(proj::tile_bounds_merc(0, 0, 0)),
                             10)}));
  }

  SpatialDB* db;
  checked(SpatialDB::Open(SpatialDBOptions(), kDatabasePath, &db));

  FeatureSet feature;
  feature.Set("type", "dummy");

  auto const cities = load_cities("/data/osm/hessen-latest.osm.pbf");
  for (auto const& city : cities) {
    FeatureSet feature;
    feature.Set("name", city.name_);
    feature.Set("layer", std::string{"cities"});
    std::cout << "insert " << city.name_ << " @ ";

    auto const xy = latlng_to_merc(city.pos_);
    std::vector<double> mem{kPointFeature, xy.x_, xy.y_};

    checked(db->Insert(WriteOptions(), bbox(xy), to_slice(mem), feature,
                       {"zoom10"}));
  }

  checked(db->Compact());

  std::cout << cities.size() << std::endl;

  return 0;
}
