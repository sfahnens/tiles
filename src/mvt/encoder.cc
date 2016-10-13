#include "tiles/mvt/encoder.h"

#include <iostream>

#include "tiles/mvt/tags.h"

#include "tiles/flat_geometry.h"

using namespace geo;
using namespace rocksdb;
using namespace protozero;

// TODO:
// - clip
// - simplify
// - polygons

namespace tiles {

enum command { MOVE_TO = 1, LINE_TO = 2, CLOSE_PATH = 7 };

uint32_t encode_command(command cmd, uint32_t count) {
  return (cmd & 0x7) | (count << 3);
}

struct delta_appender {
  delta_appender(std::vector<uint32_t>& vec, uint32_t const z,
                 pixel_coord_t const start)
      : vec_(vec), z_(z), cur_(start) {}

  void append_x(uint32_t const val_merc) {
    append(proj::merc_to_pixel_x(val_merc, z_));
  }

  void append_y(uint32_t const val_merc) {
    append(proj::merc_to_pixel_y(val_merc, z_));
  }

private:
  void append(uint32_t const val_px) {
    vec_.emplace_back(encode_zigzag32(static_cast<int32_t>(val_px - cur_)));
    cur_ = val_px;
  }

  std::vector<uint32_t>& vec_;
  uint32_t z_;
  pixel_coord_t cur_;
};

std::vector<uint32_t> encode_point(Slice const& slice, tile_spec const& spec) {
  size_t const size = get_size(slice);

  std::vector<uint32_t> vec;
  vec.reserve(size);
  vec.emplace_back(encode_command(MOVE_TO, size / 2));

  auto x_appender = delta_appender{vec, spec.z_, spec.pixel_bounds_.minx_};
  auto y_appender = delta_appender{vec, spec.z_, spec.pixel_bounds_.miny_};

  for (auto i = 1u; i < size - 1; i += 2) {
    x_appender.append_x(get_value(slice, i));
    y_appender.append_y(get_value(slice, i + 1));
  }

  return vec;
}

std::vector<uint32_t> encode_polyline(Slice const& slice,
                                      tile_spec const& spec) {
  size_t const size = get_size(slice);

  std::vector<uint32_t> vec;
  vec.reserve(size + 8);  // need to reallocate only if >8-way multi-linestring

  auto x_appender = delta_appender{vec, spec.z_, spec.pixel_bounds_.minx_};
  auto y_appender = delta_appender{vec, spec.z_, spec.pixel_bounds_.miny_};

  size_t line_size = 0;
  enum { MOVE_X, MOVE_Y, LINE_X, LINE_Y, INVALID } state = INVALID;

  for (auto i = 0u; i < size; ++i) {
    if (is_special(slice, i)) {
      vec.emplace_back(encode_command(MOVE_TO, 1));
      line_size = get_count(slice, i);
      state = MOVE_X;
      continue;
    }

    switch (state) {
      case MOVE_X:
        x_appender.append_x(get_value(slice, i));
        state = MOVE_Y;
        break;
      case MOVE_Y:
        y_appender.append_y(get_value(slice, i));
        vec.emplace_back(encode_command(LINE_TO, line_size - 1));
        state = LINE_X;
        break;
      case LINE_X:
        x_appender.append_x(get_value(slice, i));
        state = LINE_Y;
        break;
      case LINE_Y:
        y_appender.append_y(get_value(slice, i));
        state = LINE_X;
        break;
      default: assert(false);
    }
  }

  return vec;
}

std::vector<uint32_t> encode_polygon(Slice const&, tile_spec const&) {
  return {};
}

std::pair<tags::GeomType, std::vector<uint32_t>> encode_geometry(
    Slice const& slice, tile_spec const& spec) {
  auto const& type = get_type(slice);
  if (type == feature_type::POINT) {
    // std::cout << "rendering point" << std::endl;
    return {tags::GeomType::POINT, encode_point(slice, spec)};
  } else if (type == feature_type::POLYLINE) {
    // std::cout << "rendering polyline" << std::endl;
    return {tags::GeomType::LINESTRING, encode_polyline(slice, spec)};
  } else if (type == feature_type::POLYGON) {
    // std::cout << "rendering polygon" << std::endl;
    return {tags::GeomType::POLYGON, encode_polygon(slice, spec)};
  } else {
    std::cout << "unknown feature" << std::endl;
    return {tags::GeomType::UNKNOWN, {}};
  }
}

}  // namespace tiles
