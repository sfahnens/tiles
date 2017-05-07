#include "tiles/fixed/serialize.h"

#include "boost/numeric/conversion/cast.hpp"

#include "tiles/fixed/delta.h"
#include "tiles/fixed/tags.h"

namespace pz = protozero;

namespace tiles {

std::string serialize(fixed_xy const& point) {
  std::string buffer;
  pz::pbf_builder<tags::FixedGeometry> pb(buffer);

  pb.add_enum(tags::FixedGeometry::required_FixedGeometryType_type,
              tags::FixedGeometryType::POINT);

  {
    pz::packed_field_sint32 sw{
        pb, static_cast<pz::pbf_tag_type>(
                tags::FixedGeometry::packed_sint32_geometry)};
    sw.add_element(point.x_ - kFixedCoordMagicOffset);
    sw.add_element(point.y_ - kFixedCoordMagicOffset);
  }
  return buffer;
}

std::string serialize(fixed_polyline const& polyline) {
  std::string buffer;
  pz::pbf_builder<tags::FixedGeometry> pb(buffer);

  pb.add_enum(tags::FixedGeometry::required_FixedGeometryType_type,
              tags::FixedGeometryType::POLYLINE);

  {
    pz::packed_field_sint32 sw{
        pb, static_cast<pz::pbf_tag_type>(
                tags::FixedGeometry::packed_sint32_geometry)};

    delta_encoder x_encoder{kFixedCoordMagicOffset};
    delta_encoder y_encoder{kFixedCoordMagicOffset};

    sw.add_element(
        boost::numeric_cast<fixed_delta_t>(polyline.geometry_.size()));
    for (auto const& point : polyline.geometry_) {
      sw.add_element(x_encoder.encode(point.x_));
      sw.add_element(y_encoder.encode(point.y_));
    }
  }
  return buffer;
}

}  // namespace tiles
