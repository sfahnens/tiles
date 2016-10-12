#include "tiles/mvt/encoder.h"

#include <iostream>

#include "tiles/mvt/tags.h"

#include "tiles/flat_geometry.h"

using namespace rocksdb;

namespace tiles {

enum command { MOVE_TO = 1, LINE_TO = 2, CLOSE_PATH = 7 };

uint32_t encode_command(command cmd, uint32_t count) {
  return (cmd & 0x7) | (count << 3);
}

std::vector<uint32_t> encode_point(Slice const& slice, tile_spec const& spec) {
  // assert(get_count(slice) == 3, "invalid point");  // XXX MULTÍPOINT!

  auto const x = (proj::merc_to_pixel_x(get_elem(slice, 1), spec.z_) -
                  spec.px_bounds_.minx_) /
                 (spec.px_bounds_.maxx_ - spec.px_bounds_.minx_) * 4096;

  auto const y = (proj::merc_to_pixel_y(get_elem(slice, 2), spec.z_) -
                  spec.px_bounds_.miny_) /
                 (spec.px_bounds_.maxy_ - spec.px_bounds_.miny_) * 4096;

  std::cout << "point at" <<  x << " " << y << std::endl;

  return {encode_command(MOVE_TO, 1), protozero::encode_zigzag32(x),
          protozero::encode_zigzag32(y)};
}

std::vector<uint32_t> encode_polyline(Slice const&, tile_spec const&) {
  return {};
}

std::vector<uint32_t> encode_polygon(Slice const&, tile_spec const&) {
  return {};
}

std::pair<tags::GeomType, std::vector<uint32_t>> encode_geometry(
    Slice const& slice, tile_spec const& spec) {
  auto const& type = get_type(slice);
  if (type == kPointFeature) {
    std::cout << "rendering point" << std::endl;
    return {tags::GeomType::POINT, encode_point(slice, spec)};
  } else if (type == kPolylineFeature) {
    std::cout << "rendering polyline" << std::endl;
    return {tags::GeomType::LINESTRING, encode_polyline(slice, spec)};
  } else if (type == kPolygonFeature) {
    std::cout << "rendering polygon" << std::endl;
    return {tags::GeomType::POLYGON, encode_polygon(slice, spec)};
  } else {
    std::cout << "unknown feature" << std::endl;
    return {tags::GeomType::UNKNOWN, {}};
  }
}

}  // namespace tiles
