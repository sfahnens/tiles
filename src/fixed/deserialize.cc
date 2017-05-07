#include "tiles/fixed/deserialize.h"

#include "protozero/pbf_message.hpp"

#include "tiles/fixed/tags.h"
#include "tiles/fixed/delta.h"

#include "tiles/util.h"

namespace pz = protozero;

namespace tiles {

template <typename ITPair>
fixed_delta_t get_next(ITPair& it_pair) {
  verify(it_pair.first != it_pair.second, "iterator problem");
  auto val = *it_pair.first;
  ++it_pair.first;
  return val;
}

fixed_xy deserialize_point(pz::pbf_message<tags::FixedGeometry>&& msg) {
  verify(msg.next(), "invalid message");
  verify(msg.tag() == tags::FixedGeometry::packed_sint32_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint32();

  auto x = get_next(it_pair);
  auto y = get_next(it_pair);

  return fixed_xy{static_cast<fixed_coord_t>(x + kFixedCoordMagicOffset),
                  static_cast<fixed_coord_t>(y + kFixedCoordMagicOffset)};
}

fixed_polyline deserialize_polyline(
    pz::pbf_message<tags::FixedGeometry>&& msg) {
  verify(msg.next(), "invalid message");
  verify(msg.tag() == tags::FixedGeometry::packed_sint32_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint32();

  fixed_polyline result;
  result.geometry_.resize(get_next(it_pair));

  delta_decoder x_decoder{kFixedCoordMagicOffset};
  delta_decoder y_decoder{kFixedCoordMagicOffset};

  for (auto i = 0u; i < result.geometry_.size(); ++i) {
    result.geometry_[i].x_ = x_decoder.decode(get_next(it_pair));
    result.geometry_[i].y_ = y_decoder.decode(get_next(it_pair));
  }

  return result;
}

fixed_geometry deserialize(std::string const& str) {
  pz::pbf_message<tags::FixedGeometry> msg{str};

  verify(msg.next(), "1invalid message");
  verify(msg.tag() == tags::FixedGeometry::required_FixedGeometryType_type,
         "1invalid tag");

  auto type = static_cast<tags::FixedGeometryType>(msg.get_enum());
  switch (type) {
    case tags::FixedGeometryType::POINT:
      return deserialize_point(std::move(msg));
    case tags::FixedGeometryType::POLYLINE:
      return deserialize_polyline(std::move(msg));
    default: verify(false, "unknown geometry");
  }
}

}  // namespace tiles
