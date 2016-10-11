#pragma once

namespace tiles {

inline rocksdb::spatial::BoundingBox<double> bbox(bounds const& b) {
  return {b.minx_, b.miny_, b.maxx_, b.maxy_};
}

inline rocksdb::spatial::BoundingBox<double> bbox(uint32_t const x,
                                                  uint32_t const y,
                                                  uint32_t const z) {
  return bbox(proj::tile_bounds(x, y, z));
}

inline rocksdb::spatial::BoundingBox<double> bbox(meters const& m) {
  return {m.x_, m.y_, m.x_, m.y_};
}

}  // namespace tiles
