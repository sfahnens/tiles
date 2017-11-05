#include "tiles/fixed/io/deserialize.h"

#include "protozero/pbf_message.hpp"

#include "tiles/fixed/algo/delta.h"
#include "tiles/fixed/io/tags.h"
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
  verify(msg.tag() == tags::FixedGeometry::packed_sint64_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint64();

  auto x = get_next(it_pair);
  auto y = get_next(it_pair);

  return fixed_xy{static_cast<fixed_coord_t>(x + kFixedCoordMagicOffset),
                  static_cast<fixed_coord_t>(y + kFixedCoordMagicOffset)};
}

fixed_polyline deserialize_polyline(
    pz::pbf_message<tags::FixedGeometry>&& msg) {
  verify(msg.next(), "invalid message");
  verify(msg.tag() == tags::FixedGeometry::packed_sint64_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint64();

  fixed_polyline result;
  result.geometry_.emplace_back();
  result.geometry_[0].resize(get_next(it_pair));

  delta_decoder x_decoder{kFixedCoordMagicOffset};
  delta_decoder y_decoder{kFixedCoordMagicOffset};

  for (auto i = 0u; i < result.geometry_[0].size(); ++i) {
    result.geometry_[0][i].x_ = x_decoder.decode(get_next(it_pair));
    result.geometry_[0][i].y_ = y_decoder.decode(get_next(it_pair));
  }

  return result;
}

fixed_polygon deserialize_polygon(pz::pbf_message<tags::FixedGeometry>&& msg) {
  verify(msg.next(), "invalid message");
  verify(msg.tag() == tags::FixedGeometry::packed_sint64_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint64();

  delta_decoder x_decoder{kFixedCoordMagicOffset};
  delta_decoder y_decoder{kFixedCoordMagicOffset};

  fixed_polygon result;

  auto const total_size = get_next(it_pair);
  fixed_delta_t processed_size = 0;

  while (processed_size < total_size) {
    auto const prefix = get_next(it_pair);
    result.type_.emplace_back(prefix > 0);

    auto const size = std::abs(prefix);
    processed_size += size;

    result.geometry_.emplace_back(size);
    for (auto i = 0u; i < size; ++i) {
      result.geometry_.back()[i].x_ = x_decoder.decode(get_next(it_pair));
      result.geometry_.back()[i].y_ = y_decoder.decode(get_next(it_pair));
    }
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
    case tags::FixedGeometryType::POLYGON:
      return deserialize_polygon(std::move(msg));
    default: verify(false, "unknown geometry");
  }
}

}  // namespace tiles
