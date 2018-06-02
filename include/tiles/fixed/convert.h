#pragma once

#include "geo/latlng.h"
#include "geo/webmercator.h"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

inline fixed_xy latlng_to_fixed(geo::latlng const& pos) {
  auto const merc_xy =
      proj::merc_to_pixel(geo::latlng_to_merc(pos), kFixedDefaultZoomLevel);
  return {static_cast<fixed_coord_t>(std::min(
              merc_xy.x_, static_cast<geo::pixel_coord_t>(kFixedCoordMax-1))),
          static_cast<fixed_coord_t>(std::min(
              merc_xy.y_, static_cast<geo::pixel_coord_t>(kFixedCoordMax-1)))};
}

inline geo::latlng fixed_to_latlng(fixed_xy const& pos) {
  return geo::merc_to_latlng(
      {proj::pixel_to_merc_x(pos.x(), kFixedDefaultZoomLevel),
       proj::pixel_to_merc_y(pos.y(), kFixedDefaultZoomLevel)});
}

}  // namespace tiles
