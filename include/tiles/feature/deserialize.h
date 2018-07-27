#pragma once

#include <optional>

#include "protozero/pbf_message.hpp"

#include "tiles/feature/feature.h"
#include "tiles/fixed/io/deserialize.h"
#include "tiles/util.h"

namespace tiles {

inline std::optional<feature> deserialize_feature(
    std::string_view const& str,  //
    fixed_box const& box_hint = {{kInvalidBoxHint, kInvalidBoxHint},
                                 {kInvalidBoxHint, kInvalidBoxHint}},
    uint32_t const zoom_level_hint = kInvalidZoomLevel) {
  namespace pz = protozero;

  pz::pbf_message<tags::Feature> msg{str.data(), str.size()};

  uint64_t id = 0;
  std::pair<uint32_t, uint32_t> zoom_levels{kInvalidZoomLevel,
                                            kInvalidZoomLevel};

  auto const box_mask = (static_cast<uint64_t>(1) << 32) - 1;

  size_t meta_fill = 0;
  std::vector<std::pair<std::string, std::string>> meta;

  fixed_geometry geometry;

  while (msg.next()) {
    switch (msg.tag()) {
      case tags::Feature::required_uint32_minzoomlevel:
        zoom_levels.first = msg.get_uint32();
        if (zoom_level_hint != kInvalidZoomLevel &&
            zoom_levels.first > zoom_level_hint) {
          return std::nullopt;
        }
        break;
      case tags::Feature::required_uint32_maxzoomlevel:
        zoom_levels.second = msg.get_uint32();
        if (zoom_level_hint != kInvalidZoomLevel &&
            zoom_levels.second < zoom_level_hint) {
          return std::nullopt;
        }
        break;
      case tags::Feature::required_uint64_box_x: {
        auto const packed_box_x = msg.get_uint64();
        fixed_coord_t const min_x = packed_box_x & box_mask;
        fixed_coord_t const max_x = (packed_box_x >> 32) & box_mask;

        if (box_hint.min_corner().x() != kInvalidBoxHint &&
            box_hint.max_corner().x() != kInvalidBoxHint &&
            (max_x < box_hint.min_corner().x() ||
             min_x > box_hint.max_corner().x())) {
          return std::nullopt;
        }
      } break;
      case tags::Feature::required_uint64_box_y: {
        auto const packed_box_y = msg.get_uint64();
        fixed_coord_t const min_y = packed_box_y & box_mask;
        fixed_coord_t const max_y = (packed_box_y >> 32) & box_mask;

        if (box_hint.min_corner().y() != kInvalidBoxHint &&
            box_hint.max_corner().y() != kInvalidBoxHint &&
            (max_y < box_hint.min_corner().y() ||
             min_y > box_hint.max_corner().y())) {
          return std::nullopt;
        }
      } break;
      case tags::Feature::required_uint64_id: id = msg.get_uint64(); break;
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

  return feature{id, zoom_levels,
                 std::map<std::string, std::string>{begin(meta), end(meta)},
                 std::move(geometry)};
}

}  // namespace tiles
