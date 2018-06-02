#include "tiles/osm/load_coastlines.h"

#include "tiles/db/insert_feature.h"
#include "tiles/db/tile_database.h"
#include "tiles/osm/load_shapefile.h"

namespace tiles {

void load_coastlines(tile_db_handle& handle, std::string const& fname) {
  feature_inserter inserter{handle, &tile_db_handle::features_dbi};

  auto coastlines = load_shapefile(fname);
  std::cout << "loaded " << coastlines.size() << " coastlines" << std::endl;

  size_t count = 0;
  for (auto& coastline : coastlines) {
    if (count % 100 == 0) {
      std::cout << "insert coastline: " << count << "\n";
    }
    ++count;

    insert_recursive_clipped_feature(
        inserter, feature{0ul,
                          std::pair<uint32_t, uint32_t>{0, kMaxZoomLevel + 1},
                          {{"layer", "coastline"}},
                          std::move(coastline)});
  }
}

}  // namespace tiles
