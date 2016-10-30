#pragma once

#include "tiles/geo/flat_geometry.h"
#include "tiles/geo/pixel32.h"

namespace tiles {

std::vector<flat_geometry> make_point(pixel32_xy const&) {
  // return std::vector<flat_geometry>{flat_geometry{feature_type::POINT},
  //                                   flat_geometry{xy.x_}, flat_geometry{xy.y_}};
  return {};
}


}  // namespace tiles
