#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "protozero/pbf_builder.hpp"
#include "protozero/pbf_message.hpp"
#include "protozero/pbf_writer.hpp"
#include "protozero/types.hpp"
#include "protozero/varint.hpp"

using namespace protozero;

namespace tiles {
namespace tags {

enum GeometryType : int { UNKNOWN = 0, POINT = 1, POLYLINE = 2, POLYGON = 3 };

enum class CoordSeq : protozero::pbf_tag_type {
  required_fixed_uint32_x = 1,
  required_fixed_uint32_y = 2,
  packed_uint64_offsets = 3  // zigzag/varint encoded
};

enum class Geometry : protozero::pbf_tag_type {
  required_GeometryType_type = 1,
  repeated_CoordSeq_seq = 2
};

}  // namespace tags
}  // namespace tiles

using namespace tiles;

int main() {

  std::string message;

  {
    pbf_builder<tags::Geometry> pb{message};

    pb.add_enum(tags::Geometry::required_GeometryType_type, tags::POLYLINE);

    {
      pbf_builder<tags::CoordSeq> geo_pb(pb,
                                         tags::Geometry::repeated_CoordSeq_seq);

      geo_pb.add_fixed32(tags::CoordSeq::required_fixed_uint32_x, 23);
      geo_pb.add_fixed32(tags::CoordSeq::required_fixed_uint32_y, 42);

      std::vector<uint64_t> offsets;
      offsets.push_back(encode_zigzag64(123));
      offsets.push_back(encode_zigzag64(-321));
      geo_pb.add_packed_uint64(tags::CoordSeq::packed_uint64_offsets,
                               begin(offsets), end(offsets));
    }
  }
  // std::cout << std::hex << std::setfill('0') << message << std::endl;

  for (auto const& c : message) {
    printf("%02x", c);
  }
  std::cout << std::endl;

  // pbf_message<tags::Geometry> pm{message};
  // while (pm.next()) {
  //   switch (pm.tag()) {
  //     case tags::Geometry::required_GeometryType_type:
  //       std::cout << pm.get_enum() << std::endl;
  //       break;
  //     case tags::Geometry::repeated_CoordSeq_seq:
  //       auto 

  //     break;

  //     default: pm.skip();
  //   }
  // }
}