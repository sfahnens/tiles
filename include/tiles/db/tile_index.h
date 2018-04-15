#pragma once

#include "geo/tile.h"

#include "tiles/constants.h"
#include "tiles/fixed/algo/shift.h"
#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

using tile_index_t = uint64_t;

inline geo::tile_range make_tile_range(fixed_box /*copy*/ box, uint32_t z) {
  shift(box, z);

  uint32_t const x_1 = box.min_corner().x() / kTileSize;
  uint32_t const y_1 = box.min_corner().y() / kTileSize;
  uint32_t const x_2 = box.max_corner().x() / kTileSize;
  uint32_t const y_2 = box.max_corner().y() / kTileSize;

  return geo::make_tile_range(x_1, y_1, x_2, y_2, z);
}

inline tile_index_t make_feature_key(uint32_t const x, uint32_t const y,
                                     uint32_t const z, size_t const idx = 0) {
  auto const coord_bits = z;
  auto const idx_bits = sizeof(tile_index_t) * 8 - 2 * z;

  auto const coord_mask = (static_cast<tile_index_t>(1) << z) - 1;
  auto const idx_mask = (static_cast<tile_index_t>(1) << idx_bits) - 1;

  assert((x & coord_mask) == x);
  assert((y & coord_mask) == y);
  assert((idx & idx_mask) == idx);

  tile_index_t key = 0;
  key |= (x & coord_mask) << (coord_bits + idx_bits);
  key |= (y & coord_mask) << idx_bits;
  key |= idx & idx_mask;
  return key;
}

inline tile_index_t make_feature_key(geo::tile const& t, size_t const idx) {
  return make_feature_key(t.x_, t.y_, t.z_, idx);
}

}  // namespace tiles
