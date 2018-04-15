#pragma once

#include "protozero/pbf_message.hpp"

#include "tiles/feature/feature.h"
#include "tiles/fixed/io/deserialize.h"
#include "tiles/util.h"

namespace tiles {

feature deserialize_feature(std::string_view const& str) {
  namespace pz = protozero;

  pz::pbf_message<tags::Feature> msg{str.data(), str.size()};

  size_t meta_fill = 0;
  std::vector<std::pair<std::string, std::string>> meta;

  fixed_geometry geometry;

  while (msg.next()) {
    switch (msg.tag()) {
      case tags::Feature::repeated_string_keys:
        meta.emplace_back(msg.get_string(), "");
        break;
      case tags::Feature::repeated_string_values:
        verify(meta_fill < meta.size(), "meta data imbalance! (a)");
        meta[meta_fill++].second = msg.get_string();
        break;
      case tags::Feature::required_FixedGeometry_geometry:
        geometry = deserialize(msg.get_string());
        break;
      default: msg.skip();
    }
  }

  verify(meta_fill == meta.size(), "meta data imbalance! (b)");

  return feature{std::map<std::string, std::string>{begin(meta), end(meta)},
                 std::move(geometry)};
}

}  // namespace tiles