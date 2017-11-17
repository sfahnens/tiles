#include "tiles/mvt/encoder.h"

#include <iostream>

#include "tiles/mvt/tags.h"

#include "tiles/fixed/algo/delta.h"

#include "tiles/util.h"

using namespace geo;
using namespace rocksdb;
using namespace protozero;
namespace pz = protozero;

namespace tiles {

enum command { MOVE_TO = 1, LINE_TO = 2, CLOSE_PATH = 7 };

uint32_t encode_command(command cmd, uint32_t count) {
  return (cmd & 0x7) | (count << 3);
}

constexpr auto geometry_tag =
    static_cast<pz::pbf_tag_type>(tags::Feature::packed_uint32_geometry);

void encode(pz::pbf_builder<tags::Feature>&, fixed_null const&,
            tile_spec const&) {}

void encode(pz::pbf_builder<tags::Feature>& pb, fixed_point const& point,
            tile_spec const& spec) {
  pb.add_enum(tags::Feature::optional_GeomType_type, tags::GeomType::POINT);

  delta_encoder x_enc{static_cast<fixed_coord_t>(spec.pixel_bounds_.minx_)};
  delta_encoder y_enc{static_cast<fixed_coord_t>(spec.pixel_bounds_.miny_)};

  {
    pz::packed_field_uint32 sw{pb, geometry_tag};
    sw.add_element(encode_command(MOVE_TO, point.size()));
    for (auto const& p : point) {
      sw.add_element(encode_zigzag32(x_enc.encode(p.x())));
      sw.add_element(encode_zigzag32(y_enc.encode(p.y())));
    }
  }
}

template <typename Container>
void encode_path(pz::packed_field_uint32& sw, delta_encoder& x_enc,
                 delta_encoder& y_enc, Container const& c) {
  verify(c.size() > 1, "container polyline");

  sw.add_element(encode_command(MOVE_TO, 1));
  sw.add_element(encode_zigzag32(x_enc.encode(c.front().x())));
  sw.add_element(encode_zigzag32(y_enc.encode(c.front().y())));

  sw.add_element(encode_command(LINE_TO, c.size() - 1));
  for (auto i = 1u; i < c.size(); ++i) {
    sw.add_element(encode_zigzag32(x_enc.encode(c[i].x())));
    sw.add_element(encode_zigzag32(y_enc.encode(c[i].y())));
  }
}

void encode(pz::pbf_builder<tags::Feature>& pb,
            fixed_polyline const& multi_polyline, tile_spec const& spec) {
  pb.add_enum(tags::Feature::optional_GeomType_type,
              tags::GeomType::LINESTRING);

  delta_encoder x_enc{static_cast<fixed_coord_t>(spec.pixel_bounds_.minx_)};
  delta_encoder y_enc{static_cast<fixed_coord_t>(spec.pixel_bounds_.miny_)};

  {
    pz::packed_field_uint32 sw{pb, geometry_tag};

    for (auto const& polyline : multi_polyline) {
      encode_path(sw, x_enc, y_enc, polyline);
    }
  }
}

void encode(pz::pbf_builder<tags::Feature>& pb,
            fixed_polygon const& multi_polygon, tile_spec const& spec) {
  pb.add_enum(tags::Feature::optional_GeomType_type, tags::GeomType::POLYGON);

  delta_encoder x_enc{static_cast<fixed_coord_t>(spec.pixel_bounds_.minx_)};
  delta_encoder y_enc{static_cast<fixed_coord_t>(spec.pixel_bounds_.miny_)};

  verify(!multi_polygon.empty(), "multi_polygon empty");
  {
    pz::packed_field_uint32 sw{pb, geometry_tag};

    for (auto const& polygon : multi_polygon) {
      encode_path(sw, x_enc, y_enc, polygon.outer());
      sw.add_element(encode_command(CLOSE_PATH, 1));

      for (auto const& inner : polygon.inners()) {
        encode_path(sw, x_enc, y_enc, inner);
        sw.add_element(encode_command(CLOSE_PATH, 1));
      }
    }
  }
}

void encode_geometry(pz::pbf_builder<tags::Feature>& pb,
                     fixed_geometry const& geometry, tile_spec const& spec) {
  std::visit([&](auto const& arg) { encode(pb, arg, spec); }, geometry);
}

}  // namespace tiles
