#pragma once

#include <limits>

#include "boost/variant.hpp"

#include "tiles/globals.h"

namespace tiles {

using fixed_coord_t = uint32_t;
using fixed_xy = geo::xy<fixed_coord_t>;

constexpr auto kFixedCoordMin = std::numeric_limits<fixed_coord_t>::min();
constexpr auto kFixedCoordMax = std::numeric_limits<fixed_coord_t>::max();

constexpr fixed_coord_t kFixedCoordMagicOffset = kFixedCoordMax / 2ul;

struct fixed_null_geometry {};

struct polyline_tag;
struct polygon_tag;

template <typename Tag>
struct fixed_simple_container {
  std::vector<fixed_xy> geometry_;
};

using fixed_polyline = fixed_simple_container<polyline_tag>;
using fixed_polygon = fixed_simple_container<polygon_tag>;

using fixed_geometry = boost::variant<fixed_null_geometry, fixed_xy,
                                      fixed_polyline, fixed_polygon>;

inline fixed_xy latlng_to_fixed(geo::latlng const& pos) {
  constexpr int64_t kMax = std::numeric_limits<uint32_t>::max();

  auto const px = proj::merc_to_pixel(latlng_to_merc(pos), proj::kMaxZoomLevel);
  return {static_cast<fixed_coord_t>(std::min(px.x_, kMax)),
          static_cast<fixed_coord_t>(std::min(px.y_, kMax))};
}

}  // namespace tiles
