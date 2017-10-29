#pragma once

#include <thread>

#include "boost/filesystem.hpp"

#include "rocksdb/slice.h"
#include "rocksdb/utilities/spatial_db.h"

#include "tiles/mvt/builder.h"
#include "tiles/tile_spec.h"

namespace tiles {

constexpr auto kPrepTilesColumnFamilyName = "tiles-prep-tiles";
constexpr auto kInvalidPrepMaxZoomLevel = std::numeric_limits<uint32_t>::max();

struct cf_holder {
  explicit cf_holder(std::string name, rocksdb::ColumnFamilyOptions options =
                                           rocksdb::ColumnFamilyOptions{})
      : desc_(name, options) {}

  rocksdb::ColumnFamilyDescriptor desc_;
  std::unique_ptr<rocksdb::ColumnFamilyHandle> handle_;
};

struct tile_database {
  using error_handler_t = std::function<void(std::string const&)>;

  tile_database(error_handler_t error_handler)
      : error_handler_(std::move(error_handler)),
        prep_tiles_cf_(kPrepTilesColumnFamilyName),
        prep_max_zoom_level_(kInvalidPrepMaxZoomLevel) {}

  void verify_database_open() const {
    if (!db_) {
      error_handler_("tile_database: database not open");
    }
  }

  void verify_status(rocksdb::Status const& status) const {
    if (!status.ok()) {
      error_handler_(status.ToString());
    }
  }

  void put_feature(rocksdb::spatial::BoundingBox<double> const&,
                   rocksdb::Slice const&, rocksdb::spatial::FeatureSet const&,
                   std::vector<std::string> const&);

  std::string get_tile(tile_spec const&, tile_builder::config const& tb_config =
                                             tile_builder::config{}) const;

  void prepare_tiles(uint32_t max_z = 10);

  void compact(int num_threads = std::thread::hardware_concurrency());

  error_handler_t error_handler_;

  std::unique_ptr<rocksdb::spatial::SpatialDB> db_;
  cf_holder prep_tiles_cf_;
  uint32_t prep_max_zoom_level_;
};

std::unique_ptr<tile_database> make_tile_database(
    std::string const& path, bool read_only, bool truncate,
    std::vector<cf_holder*> const& extra_cfs,
    tile_database::error_handler_t error_handler = [](std::string const& msg) {
      std::cout << "tile_database error: " << msg << std::endl;
      std::exit(1);
    });

}  // namespace tiles
