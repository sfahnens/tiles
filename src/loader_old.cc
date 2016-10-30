#include <cstdlib>
#include <iostream>
#include <limits>

#include "boost/filesystem.hpp"

#include "rocksdb/utilities/spatial_db.h"

#include "tiles/data.h"
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

  auto const cities = load_cities("/data/osm/hessen-latest.osm.pbf");
  for (auto const& city : cities) {
    FeatureSet feature;
    feature.Set("name", city.name_);
    feature.Set("layer", std::string{"cities"});
    std::cout << "insert " << city.name_ << " @ ";

    auto const xy = latlng_to_merc(city.pos_);
    std::vector<flat_geometry> mem{flat_geometry{feature_type::POINT},
                                   flat_geometry{xy.x_},
                                   flat_geometry{xy.y_}};

    checked(db->Insert(WriteOptions(), bbox(xy), to_slice(mem), feature,
                       {"zoom10"}));
  }
  std::cout << cities.size() << std::endl;

  auto const railways = load_railways("/data/osm/hessen-latest.osm.pbf");
  for (auto const& railway : railways) {

    if (railway.size() < 2) {
      continue;
    }

    FeatureSet feature;
    feature.Set("layer", std::string{"rail"});

    std::vector<flat_geometry> mem{
        flat_geometry{feature_type::POLYLINE, railway.size()}};
    mem.reserve(railway.size() * 2 + 1);

    double minx = std::numeric_limits<double>::infinity();
    double miny = std::numeric_limits<double>::infinity();
    double maxx = -std::numeric_limits<double>::infinity();
    double maxy = -std::numeric_limits<double>::infinity();

    for (auto const& pos : railway) {
      auto const xy = latlng_to_merc(pos);
      mem.push_back(flat_geometry{xy.x_});
      mem.push_back(flat_geometry{xy.y_});

      minx = xy.x_ < minx ? xy.x_ : minx;
      miny = xy.y_ < miny ? xy.y_ : miny;
      maxx = xy.x_ > maxx ? xy.x_ : maxx;
      maxy = xy.y_ > maxy ? xy.y_ : maxy;
    }

    checked(db->Insert(WriteOptions(), {minx, miny, maxx, maxy}, to_slice(mem),
                       feature, {"zoom10"}));
  }

  checked(db->Compact());

  return 0;
}
