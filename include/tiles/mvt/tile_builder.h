#pragma once

#include <memory>

#include "geo/tile.h"

#include "tiles/feature/feature.h"

namespace tiles {

struct render_ctx;

struct tile_builder {
  tile_builder(render_ctx const&, geo::tile const&);
  ~tile_builder();

  void add_feature(feature const&);

  std::string finish();

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
