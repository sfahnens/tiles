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

template <typename ITPair, typename Points>
void deserialize_points(ITPair& it_pair, delta_decoder& x_dec,
                        delta_decoder& y_dec, Points& points) {
  auto const size = get_next(it_pair);
  points.reserve(size);

  for (auto i = 0u; i < size; ++i) {
    points.emplace_back(x_dec.decode(get_next(it_pair)),
                        y_dec.decode(get_next(it_pair)));
  }
}

fixed_point deserialize_point(pz::pbf_message<tags::FixedGeometry>&& msg) {
  verify(msg.next(), "invalid message");
  verify(msg.tag() == tags::FixedGeometry::packed_sint64_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint64();

  delta_decoder x_decoder{kFixedCoordMagicOffset};
  delta_decoder y_decoder{kFixedCoordMagicOffset};

  fixed_point point;
  deserialize_points(it_pair, x_decoder, y_decoder, point);
  return point;
}

fixed_polyline deserialize_polyline(
    pz::pbf_message<tags::FixedGeometry>&& msg) {
  verify(msg.next(), "invalid message");
  verify(msg.tag() == tags::FixedGeometry::packed_sint64_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint64();

  delta_decoder x_decoder{kFixedCoordMagicOffset};
  delta_decoder y_decoder{kFixedCoordMagicOffset};

  auto const size = get_next(it_pair);
  fixed_polyline polyline;
  polyline.resize(size);

  for (auto i = 0u; i < size; ++i) {
    deserialize_points(it_pair, x_decoder, y_decoder, polyline[i]);
  }

  return polyline;
}

fixed_polygon deserialize_polygon(pz::pbf_message<tags::FixedGeometry>&& msg) {
  verify(msg.next(), "invalid message");
  verify(msg.tag() == tags::FixedGeometry::packed_sint64_geometry,
         "invalid tag");

  auto it_pair = msg.get_packed_sint64();

  delta_decoder x_decoder{kFixedCoordMagicOffset};
  delta_decoder y_decoder{kFixedCoordMagicOffset};

  auto const size = get_next(it_pair);
  fixed_polygon polygon;
  polygon.resize(size);

  for (auto i = 0u; i < size; ++i) {
    deserialize_points(it_pair, x_decoder, y_decoder, polygon[i].outer());

    auto const inner_size = get_next(it_pair);
    polygon[i].inners().resize(inner_size);
    for (auto j = 0u; j < inner_size; ++j) {
      deserialize_points(it_pair, x_decoder, y_decoder, polygon[i].inners()[j]);
    }
  }
  return polygon;
}

fixed_geometry deserialize(std::string const& str) {
  pz::pbf_message<tags::FixedGeometry> msg{str};

  verify(msg.next(), "1invalid message");
  verify(msg.tag() == tags::FixedGeometry::required_FixedGeometryType_type,
         "1invalid tag");

  auto type = static_cast<tags::FixedGeometryType>(msg.get_enum());
  switch (type) {
    case tags::FixedGeometryType::POINT:
      std::cout << "point" << std::endl;
      return deserialize_point(std::move(msg));
    case tags::FixedGeometryType::POLYLINE:
      std::cout << "polyline" << std::endl;
      return deserialize_polyline(std::move(msg));
    case tags::FixedGeometryType::POLYGON:
      std::cout << "polygon" << std::endl;
      return deserialize_polygon(std::move(msg));
    default: verify(false, "unknown geometry");
  }
}

}  // namespace tiles
