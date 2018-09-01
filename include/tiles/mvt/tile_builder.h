#pragma once

#include <memory>
#include <string>

#include "geo/tile.h"

#include "tiles/feature/feature.h"

namespace tiles {

struct tile_builder {
  struct config {
    config() : simplify_{false}, render_debug_info_{false}, verbose_{false} {}

    bool simplify_;
    bool render_debug_info_;
    bool verbose_;
  };

  explicit tile_builder(geo::tile const&,
                        std::vector<std::string> const& layer_names,
                        config c = config());
  ~tile_builder();

  void add_feature(feature const&);

  std::string finish();

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
