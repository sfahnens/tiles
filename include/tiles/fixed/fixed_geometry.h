#pragma once

#include <limits>
#include <vector>

#include "boost/variant.hpp"

#include "geo/webmercator.h"

#include "tiles/tile_spec.h"

namespace tiles {

using fixed_coord_t = int64_t;
using fixed_xy = geo::xy<fixed_coord_t>;

constexpr fixed_coord_t kFixedCoordMin = 0;
constexpr fixed_coord_t kFixedCoordMax = proj::map_size(kMaxZoomLevel);

using fixed_delta_t = int64_t;

constexpr fixed_coord_t kFixedCoordMagicOffset = kFixedCoordMax / 2ul;

struct fixed_null_geometry {};

struct polyline_tag;
struct polygon_tag;

template <typename Tag>
struct fixed_container {
  std::vector<std::vector<fixed_xy>> geometry_;

  friend bool operator==(fixed_container<Tag> const& lhs,
                         fixed_container<Tag> const& rhs) {
    return lhs.geometry_ == rhs.geometry_;
  }
};

using fixed_polyline = fixed_container<polyline_tag>;
using fixed_polygon = fixed_container<polygon_tag>;

using fixed_geometry = boost::variant<fixed_null_geometry, fixed_xy,
                                      fixed_polyline, fixed_polygon>;

struct fixed_geometry_index {
  static int const null;
};

constexpr auto kFixedDefaultZoomLevel = 20ul;
static_assert(kFixedDefaultZoomLevel <= kMaxZoomLevel, "invalid default zoom");

inline fixed_xy latlng_to_fixed(geo::latlng const& pos) {
  auto const merc_xy =
      proj::merc_to_pixel(geo::latlng_to_merc(pos), kFixedDefaultZoomLevel);
  return {static_cast<fixed_coord_t>(std::min(
              merc_xy.x_, static_cast<geo::pixel_coord_t>(kFixedCoordMax))),
          static_cast<fixed_coord_t>(std::min(
              merc_xy.y_, static_cast<geo::pixel_coord_t>(kFixedCoordMax)))};
}

inline geo::latlng fixed_to_latlng(fixed_xy const& pos) {
  return geo::merc_to_latlng(proj::pixel_to_merc(pos, kFixedDefaultZoomLevel));
}

}  // namespace tiles
