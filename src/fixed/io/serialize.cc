#include "tiles/fixed/io/serialize.h"

#include "boost/numeric/conversion/cast.hpp"

#include "utl/zip.h"

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

std::string serialize(fixed_polygon const& polygon) {
  std::string buffer;
  pz::pbf_builder<tags::FixedGeometry> pb(buffer);

  pb.add_enum(tags::FixedGeometry::required_FixedGeometryType_type,
              tags::FixedGeometryType::POLYGON);
  {
    pz::packed_field_sint64 sw{
        pb, static_cast<pz::pbf_tag_type>(
                tags::FixedGeometry::packed_sint64_geometry)};

    delta_encoder x_encoder{kFixedCoordMagicOffset};
    delta_encoder y_encoder{kFixedCoordMagicOffset};

    verify(!polygon.geometry_.empty(), "empty polygon");
    verify(polygon.geometry_.size() == polygon.type_.size(), "invalid polygon");

    auto const total_size = std::accumulate(
        begin(polygon.geometry_), end(polygon.geometry_), 0,
        [](auto acc, auto geometry) { return acc + geometry.size(); });
    sw.add_element(boost::numeric_cast<fixed_delta_t>(total_size));

    for (auto const& ring : utl::zip(polygon.geometry_, polygon.type_)) {
      sw.add_element(
          boost::numeric_cast<fixed_delta_t>(std::get<0>(ring).size()) *
          (std::get<1>(ring) ? 1 : -1));
      for (auto const& point : std::get<0>(ring)) {
        sw.add_element(x_encoder.encode(point.x_));
        sw.add_element(y_encoder.encode(point.y_));
      }
    }
  }
  return buffer;
}

}  // namespace tiles
