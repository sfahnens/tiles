#include "tiles/fixed/io/serialize.h"

#include "boost/numeric/conversion/cast.hpp"

#include "tiles/fixed/algo/delta.h"
#include "tiles/fixed/io/tags.h"
#include "tiles/util.h"

namespace pz = protozero;

namespace tiles {

std::string serialize(fixed_xy const& point) {
  std::string buffer;
  pz::pbf_builder<tags::FixedGeometry> pb(buffer);

  pb.add_enum(tags::FixedGeometry::required_FixedGeometryType_type,
              tags::FixedGeometryType::POINT);

  {
    pz::packed_field_sint64 sw{
        pb, static_cast<pz::pbf_tag_type>(
                tags::FixedGeometry::packed_sint64_geometry)};
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

    // TODO this is zigzag64!!!!!!!!
    pz::packed_field_sint64 sw{
        pb, static_cast<pz::pbf_tag_type>(
                tags::FixedGeometry::packed_sint64_geometry)};

    delta_encoder x_encoder{kFixedCoordMagicOffset};
    delta_encoder y_encoder{kFixedCoordMagicOffset};

    verify(polyline.geometry_.size() == 1, "unsupported geometry");

    sw.add_element(
        boost::numeric_cast<fixed_delta_t>(polyline.geometry_[0].size()));
    for (auto const& point : polyline.geometry_[0]) {
      sw.add_element(x_encoder.encode(point.x_));
      sw.add_element(y_encoder.encode(point.y_));
    }
  }
  return buffer;
}

}  // namespace tiles
