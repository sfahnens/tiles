#pragma once

#include "geo/latlng.h"

#include "tiles/globals.h"

namespace tiles {

using pixel32_t = uint32_t;
using pixel32_xy = geo::xy<pixel32_t>;

pixel32_xy latlng_to_pixel32(geo::latlng const& pos) {
  auto const px = proj::merc_to_pixel(latlng_to_merc(pos), proj::kMaxZoomLevel);
  constexpr int64_t kMax = std::numeric_limits<uint32_t>::max();
  return {static_cast<pixel32_t>(std::min(px.x_, kMax)),
          static_cast<pixel32_t>(std::min(px.y_, kMax))};
}

}  // namespace tiles
