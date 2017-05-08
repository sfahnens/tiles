#include "tiles/mvt/encoder.h"

#include <iostream>

#include "tiles/mvt/tags.h"

#include "tiles/fixed/delta.h"

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

struct geometry_encoder : public boost::static_visitor<> {

  void operator()(pz::pbf_builder<tags::Feature>&, fixed_null_geometry const&,
                  tile_spec const&) const {}

  void operator()(pz::pbf_builder<tags::Feature>& pb, fixed_xy const& point,
                  tile_spec const& spec) {
    pb.add_enum(tags::Feature::optional_GeomType_type, tags::GeomType::POINT);

    {
      pz::packed_field_uint32 sw{pb, geometry_tag};
      sw.add_element(encode_command(MOVE_TO, 1));
      sw.add_element(encode_zigzag32(point.x_ - spec.pixel_bounds_.minx_));
      sw.add_element(encode_zigzag32(point.y_ - spec.pixel_bounds_.miny_));
    }
  }

  void operator()(pz::pbf_builder<tags::Feature>& pb,
                  fixed_polyline const& polyline, tile_spec const& spec) {
    pb.add_enum(tags::Feature::optional_GeomType_type,
                tags::GeomType::LINESTRING);

    delta_encoder x_encoder{
        static_cast<fixed_coord_t>(spec.pixel_bounds_.minx_)};
    delta_encoder y_encoder{
        static_cast<fixed_coord_t>(spec.pixel_bounds_.miny_)};

    verify(polyline.geometry_.size() > 1, "empty polyline");

    {
      pz::packed_field_uint32 sw{pb, geometry_tag};

      sw.add_element(encode_command(MOVE_TO, 1));
      sw.add_element(
          encode_zigzag32(x_encoder.encode(polyline.geometry_.front().x_)));
      sw.add_element(
          encode_zigzag32(y_encoder.encode(polyline.geometry_.front().y_)));

      sw.add_element(encode_command(LINE_TO, polyline.geometry_.size() - 1));
      for (auto i = 1u; i < polyline.geometry_.size(); ++i) {
        sw.add_element(
            encode_zigzag32(x_encoder.encode(polyline.geometry_[i].x_)));
        sw.add_element(
            encode_zigzag32(y_encoder.encode(polyline.geometry_[i].y_)));
      }
    }
  }

  void operator()(pz::pbf_builder<tags::Feature>&, fixed_polygon const&,
                  tile_spec const&) const {}
};

void encode_geometry(pz::pbf_builder<tags::Feature>& pb,
                     fixed_geometry const& geometry, tile_spec const& spec) {
  boost::apply_visitor(
      [&](auto const& unpacked) { geometry_encoder{}(pb, unpacked, spec); },
      geometry);
}

}  // namespace tiles
