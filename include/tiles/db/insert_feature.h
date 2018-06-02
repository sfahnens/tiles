#pragma once

#include <stack>

#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/feature/feature.h"
#include "tiles/feature/serialize.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/algo/clip.h"
#include "tiles/fixed/algo/area.h"
#include "tiles/fixed/convert.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/mvt/tile_spec.h"

namespace tiles {

inline void insert_feature(feature_inserter& inserter, feature const& f) {
  auto const box = bounding_box(f.geometry_);
  auto const value = serialize_feature(f);

  uint32_t z = 10;  // whatever
  for (auto const& tile : make_tile_range(box, z)) {
    auto const idx = inserter.fill_state_[{tile.x_, tile.y_}]++;

    auto key = make_feature_key(tile, idx);
    inserter.insert(key, value);
  }
}

inline void insert_recursive_clipped_feature(feature_inserter& inserter,
                                             feature const& f,
                                             uint32_t clip_limit = 4) {
  auto const box = bounding_box(f.geometry_);

  uint32_t const idx_z = 10;
  auto const idx_range = make_tile_range(box, idx_z);

  uint32_t clip_z = idx_z;
  auto clip_range = idx_range;
  while (clip_z > clip_limit && ++clip_range.begin() != clip_range.end()) {
    clip_range = geo::tile_range_on_z(clip_range, --clip_z);
  }

  feature clipped;
  clipped.id_ = f.id_;
  clipped.zoom_levels_ = f.zoom_levels_;
  clipped.meta_ = f.meta_;

  if (clip_z < idx_z) {

    for (auto const& tile : clip_range) {
      tile_spec spec{tile};
      clipped.geometry_ = clip(f.geometry_, spec.draw_bounds_);

      if (std::holds_alternative<fixed_null>(clipped.geometry_)) {
        continue;
      }

      if (std::holds_alternative<fixed_polygon>(clipped.geometry_)) {
        auto const& polygon = std::get<fixed_polygon>(clipped.geometry_);
        if(polygon.size() == 1 &&
           polygon.front().outer().size() == 5 &&
           polygon.front().inners().empty() &&
           area(spec.draw_bounds_) == area(polygon)) {
          std::cout << "found fully filled: " << tile << std::endl;
          continue;
        }
      }

      insert_recursive_clipped_feature(inserter, clipped, clip_limit + 1);
    }
  } else {
    for (auto const& tile : idx_range) {
      tile_spec spec{tile};
      clipped.geometry_ = clip(f.geometry_, spec.draw_bounds_);

      if (std::holds_alternative<fixed_null>(clipped.geometry_)) {
        continue;
      }
      auto const idx = inserter.fill_state_[{tile.x_, tile.y_}]++;
      auto const key = make_feature_key(tile, idx);
      auto const value = serialize_feature(clipped);
      inserter.insert(key, value);
    }
  }
}

}  // namespace tiles
