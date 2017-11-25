#pragma once

#include "geo/tile.h"

#include "tiles/fixed/algo/shift.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/constants.h"

namespace tiles {

using tile_index_t = uint64_t;

inline geo::tile_range make_tile_range(fixed_box /*copy*/ box, uint32_t z) {
  shift(box, z);

  uint32_t const x_1 = box.min_corner().x() / kTileSize;
  uint32_t const y_1 = box.min_corner().y() / kTileSize;
  uint32_t const x_2 = box.max_corner().x() / kTileSize;
  uint32_t const y_2 = box.max_corner().y() / kTileSize;

  // std::cout << tile_min_x << ", " << tile_min_y << ", " << tile_max_x << ", "
  //           << tile_max_y << std::endl;

  return geo::make_tile_range(x_1, y_1, x_2, y_2, z);
}

inline tile_index_t tile_coords_to_key(uint32_t const x, uint32_t const y) {
  constexpr tile_index_t kMask32Bit = 0xFF'FF'FF'FF;

  tile_index_t key = 0;
  key |= (x & kMask32Bit) << 32;
  key |= y & kMask32Bit;
  return key;
}

inline tile_index_t tile_to_key(geo::tile const& t) {
  return tile_coords_to_key(t.x_, t.y_);
}

}  // namespace tiles
