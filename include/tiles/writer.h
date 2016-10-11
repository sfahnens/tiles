#pragma once

#include <string>
#include <vector>

#include "protozero/pbf_builder.hpp"
#include "protozero/pbf_writer.hpp"
#include "protozero/types.hpp"
#include "protozero/varint.hpp"

namespace tiles {

// following directly:
// https://github.com/mapbox/vector-tile-spec/blob/master/2.1/vector_tile.proto
namespace tags {

enum GeomType : int {
  UNKNOWN = 0,
  POINT = 1,
  LINESTRING = 2,
  POLYGON = 3
};

enum class Value : protozero::pbf_tag_type {
  optional_string_string_value = 1,
  optional_float_float_value = 2,
  optional_double_double_valze = 3,
  optional_int64_int_value = 4, 
  optional_uint64_uint_value = 5,
  optional_sint64_sint_value = 6,
  optional_bool_bool_value = 7
};

enum class Feature : protozero::pbf_tag_type {
  optional_uint64_id = 1, 
  packed_uint32_tags = 2,
  optional_GeomType_type = 3,
  packed_uint32_geometry = 4
};

enum class Layer : protozero::pbf_tag_type {
  required_uint32_version = 15,
  required_string_name = 1,
  repeated_Feature_features = 2,
  repeated_string_keys = 3,
  repeated_Value_values = 4,
  optional_uint32_extent = 5
};

enum class Tile : protozero::pbf_tag_type {
  repeated_Layer_layers = 3
};

} // namespace tags

// std::string make_tile() {
//   std::string tile_buf;
//   protozero::pbf_builder<tags::Tile> tile_pb(tile_buf);

//   std::string feature_buf;
//   protozero::pbf_builder<tags::Feature> feature_pb(feature_buf);
//   feature_pb.add_enum(tags::Feature::optional_GeomType_type, tags::GeomType::POINT);

//   uint32_t mv_cmd = (1 & 0x7) | (1 << 3);

//   std::vector<uint32_t> point{mv_cmd, protozero::encode_zigzag32(2048), protozero::encode_zigzag32(2048)};
//   feature_pb.add_packed_uint32(tags::Feature::packed_uint32_geometry, begin(point), end(point));

//   std::string layer_buf;
//   protozero::pbf_builder<tags::Layer> layer_pb(layer_buf);
//   layer_pb.add_uint32(tags::Layer::required_uint32_version, 2);
//   layer_pb.add_string(tags::Layer::required_string_name, "yolo");
//   layer_pb.add_uint32(tags::Layer::optional_uint32_extent, 4096);

//   layer_pb.add_message(tags::Layer::repeated_Feature_features, feature_buf);

//   tile_pb.add_message(tags::Tile::repeated_Layer_layers, layer_buf);

//   return tile_buf;
// }

std::string make_tile(bounds const& boxstd::vector<meters> const& meters) {
  std::string tile_buf;
  protozero::pbf_builder<tags::Tile> tile_pb(tile_buf);

  std::string feature_buf;
  protozero::pbf_builder<tags::Feature> feature_pb(feature_buf);
  feature_pb.add_enum(tags::Feature::optional_GeomType_type, tags::GeomType::POINT);

  uint32_t mv_cmd = (1 & 0x7) | (1 << 3);

  std::vector<uint32_t> point{mv_cmd, protozero::encode_zigzag32(2048), protozero::encode_zigzag32(2048)};
  feature_pb.add_packed_uint32(tags::Feature::packed_uint32_geometry, begin(point), end(point));

  std::string layer_buf;
  protozero::pbf_builder<tags::Layer> layer_pb(layer_buf);
  layer_pb.add_uint32(tags::Layer::required_uint32_version, 2);
  layer_pb.add_string(tags::Layer::required_string_name, "yolo");
  layer_pb.add_uint32(tags::Layer::optional_uint32_extent, 4096);

  layer_pb.add_message(tags::Layer::repeated_Feature_features, feature_buf);

  tile_pb.add_message(tags::Tile::repeated_Layer_layers, layer_buf);

  return tile_buf;
}


struct layer_builder {


  std::string layer_buf;
}

struct tile_builder {
  explicit tile_builder(bounds b) : bounds_(std::move(b)) {};




  std::sting layer_buf_;
  bounds bounds_;
};


}  // namespace tiles
