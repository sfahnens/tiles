#pragma once

#include "rocksdb/utilities/spatial_db.h"

#include "tiles/globals.h"

#include "tiles/util.h"

namespace tiles {

constexpr char kDatabasePath[] = "spatial";

inline void checked(rocksdb::Status&& status) {
  if (!status.ok()) {
    std::cout << "error: " << status.ToString() << std::endl;
    std::exit(1);
  }
}

using spatial_db_ptr = std::unique_ptr<rocksdb::spatial::SpatialDB>;

inline spatial_db_ptr open_spatial_db(std::string const& path,
                                      bool read_only = false) {
  using namespace rocksdb::spatial;

  SpatialDB* db;
  checked(
      SpatialDB::Open(SpatialDBOptions(), path, &db, {}, nullptr, read_only));

  return std::unique_ptr<rocksdb::spatial::SpatialDB>(db);
}

inline rocksdb::Status create_spatial_db(
    std::string const& path, std::vector<rocksdb::ColumnFamilyDescriptor> const&
                                 extra_column_families = {}) {
  using namespace rocksdb::spatial;

  auto box = rocksdb::spatial::BoundingBox<double>{0, 0, proj::map_size(20), proj::map_size(20)};


  return SpatialDB::Create(
      SpatialDBOptions(), path,
      {SpatialIndexOptions("zoom10", box, // bbox(proj::tile_bounds_merc(0, 0, 0)),
                           10)},
      extra_column_families);
}

inline void init_spatial_db(std::string const& path) {
  checked(create_spatial_db(path));
}

}  // namespace tiles
