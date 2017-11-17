#pragma once

#include <variant>

#include "boost/geometry/geometries/box.hpp"
#include "boost/geometry/geometries/linestring.hpp"
#include "boost/geometry/geometries/multi_linestring.hpp"
#include "boost/geometry/geometries/multi_point.hpp"
#include "boost/geometry/geometries/multi_polygon.hpp"
#include "boost/geometry/geometries/point_xy.hpp"
#include "boost/geometry/geometries/polygon.hpp"

#include "geo/webmercator.h"
#include "tiles/tile_spec.h"

namespace tiles {

using fixed_coord_t = int64_t;

using fixed_xy = boost::geometry::model::d2::point_xy<fixed_coord_t>;

constexpr fixed_coord_t kFixedCoordMin = 0;
constexpr fixed_coord_t kFixedCoordMax = proj::map_size(kMaxZoomLevel);
constexpr fixed_coord_t kFixedCoordMagicOffset = kFixedCoordMax / 2ul;

constexpr auto kFixedDefaultZoomLevel = 20ul;
static_assert(kFixedDefaultZoomLevel <= kMaxZoomLevel, "invalid default zoom");

using fixed_delta_t = int64_t;

using fixed_null = std::monostate;
using fixed_point = boost::geometry::model::multi_point<fixed_xy>;
using fixed_polyline = boost::geometry::model::multi_linestring<
    boost::geometry::model::linestring<fixed_xy>>;
using fixed_polygon = boost::geometry::model::multi_polygon<
    boost::geometry::model::polygon<fixed_xy>>;

using fixed_geometry =
std::variant<fixed_null, fixed_point, fixed_polyline, fixed_polygon>;

using fixed_box = boost::geometry::model::box<fixed_xy>;

}  // namespace tiles
