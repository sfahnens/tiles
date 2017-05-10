#pragma once

#include <memory>
#include <string>

#include "rocksdb/slice.h"
#include "rocksdb/utilities/spatial_db.h"

#include "tiles/tile_spec.h"

namespace tiles {

struct tile_builder {
  explicit tile_builder(tile_spec const&);
  ~tile_builder();

  void add_feature(rocksdb::spatial::FeatureSet const& meta,
                   rocksdb::Slice const& geo);

  std::string finish();

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
