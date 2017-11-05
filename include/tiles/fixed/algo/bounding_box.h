#pragma once

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

fixed_box bounding_box(fixed_xy const&);
fixed_box bounding_box(fixed_polyline const&);
fixed_box bounding_box(fixed_polygon const&);

}  // namespace tiles
