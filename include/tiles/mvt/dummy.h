#pragma once 

#include <string>
#include <vector>

#include "tiles/mvt/tags.h"


namespace tiles {
  
std::string make_tile() {
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
  layer_pb.add_string(tags::Layer::required_string_name, "cities");
  layer_pb.add_uint32(tags::Layer::optional_uint32_extent, 4096);

  layer_pb.add_message(tags::Layer::repeated_Feature_features, feature_buf);

  tile_pb.add_message(tags::Tile::repeated_Layer_layers, layer_buf);

  return tile_buf;
}


} // namespace tiles
