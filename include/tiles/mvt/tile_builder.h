#pragma once

#include <memory>
#include <string>

#include "geo/tile.h"

#include "tiles/feature/feature.h"

namespace tiles {

struct tile_builder {
  struct config {
    config() {}

    bool simplify_ = false;
    bool render_debug_info_ = false;
    bool verbose_ = false;
  };

  explicit tile_builder(geo::tile const&, config const& = {});
  ~tile_builder();

  void add_feature(feature const&);

  std::string finish();

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
