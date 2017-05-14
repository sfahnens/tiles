#pragma once

#include <thread>

#include "boost/filesystem.hpp"

#include "rocksdb/slice.h"
#include "rocksdb/utilities/spatial_db.h"

#include "tiles/mvt/builder.h"
#include "tiles/tile_spec.h"

namespace tiles {

constexpr auto kPrepTilesColumnFamilyName = "tiles-prep-tiles";

struct cf_holder {
  explicit cf_holder(std::string name, rocksdb::ColumnFamilyOptions options =
                                           rocksdb::ColumnFamilyOptions{})
      : desc_(name, options) {}

  rocksdb::ColumnFamilyDescriptor desc_;
  std::unique_ptr<rocksdb::ColumnFamilyHandle> handle_;
};

struct tile_database {
  tile_database(std::string path, bool read_only)
      : path_(std::move(path)),
        read_only_(read_only),
        prep_tiles_cf_(kPrepTilesColumnFamilyName) {}

  void open(std::vector<cf_holder*> const& extra_cfs);

  void verify_database_open() const {
    if (!db_) {
      emit_error("tile_database: database not open");
    }
  }

  void verify_status(rocksdb::Status const& status) const {
    if (!status.ok()) {
      emit_error(status.ToString());
    }
  }

  virtual void emit_error(std::string const& msg) const {
    std::cout << "tile_database error: " << msg << std::endl;
    std::exit(1);
  }

  void put_tile(tile_spec const&, rocksdb::Slice const&);
  void put_feature(rocksdb::spatial::BoundingBox<double> const&,
                   rocksdb::Slice const&, rocksdb::spatial::FeatureSet const&,
                   std::vector<std::string> const&);

  std::string get_tile(tile_spec const&, tile_builder::config const& tb_config =
                                             tile_builder::config{}) const;

  void compact(int num_threads = std::thread::hardware_concurrency());

  std::string path_;
  bool read_only_;

  std::unique_ptr<rocksdb::spatial::SpatialDB> db_;
  cf_holder prep_tiles_cf_;

private:
  void create_database(
      std::vector<rocksdb::ColumnFamilyDescriptor> const&) const;
};

}  // namespace tiles
