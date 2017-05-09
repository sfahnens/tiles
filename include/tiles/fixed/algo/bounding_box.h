#pragma once

#include "rocksdb/utilities/spatial_db.h"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

// rocksdb::spatial::BoundingBox<fixed_coord_t> bounding_box(
//     fixed_geometry const&);

rocksdb::spatial::BoundingBox<double> bounding_box(fixed_xy const&);
rocksdb::spatial::BoundingBox<double> bounding_box(
    fixed_polyline const&);

}  // namespace tiles
