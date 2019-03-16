#pragma once

#include "protozero/pbf_builder.hpp"

#include "tiles/db/shared_strings.h"
#include "tiles/feature/feature.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/algo/delta.h"
#include "tiles/fixed/algo/make_simplify_mask.h"
#include "tiles/fixed/io/serialize.h"

namespace tiles {

inline std::string serialize_feature(feature const& f,
                                     meta_coding_map_t const& coding_map = {},
                                     bool fast = true) {
  std::string buf;
  protozero::pbf_builder<tags::Feature> pb(buf);

  auto const box = bounding_box(f.geometry_);

  // XXX: maybe tile dependent offsets would be more compact!
  delta_encoder x_enc{kFixedCoordMagicOffset};
  delta_encoder y_enc{kFixedCoordMagicOffset};

  std::array<int64_t, 7> header{{
      f.zoom_levels_.first,  // 0: min zoom level
      f.zoom_levels_.second,  // 1:  max zoom level
      x_enc.encode(box.min_corner().x()),  // 2
      x_enc.encode(box.max_corner().x()),  // 3
      y_enc.encode(box.min_corner().y()),  // 4
      y_enc.encode(box.max_corner().y()),  // 5
      static_cast<int64_t>(f.layer_)  // 6
  }};

  pb.add_packed_sint64(tags::Feature::packed_sint64_header,  //
                       begin(header), end(header));

  pb.add_uint64(tags::Feature::required_uint64_id, f.id_);

  if (!fast) {
    std::vector<size_t> coded_metas;
    std::vector<std::string> uncoded_keys, uncoded_values;

    for (auto const& pair : f.meta_) {
      auto it = coding_map.find(pair);
      if (it == end(coding_map)) {
        uncoded_keys.push_back(pair.first);
        uncoded_values.push_back(pair.second);
      } else {
        coded_metas.push_back(it->second);
      }
    }

    if (!coded_metas.empty()) {
      pb.add_packed_uint64(tags::Feature::packed_uint64_meta_pairs,  //
                           begin(coded_metas), end(coded_metas));
    }
    for (auto const& k : uncoded_keys) {
      pb.add_string(tags::Feature::repeated_string_keys, k);
    }
    for (auto const& v : uncoded_values) {
      pb.add_string(tags::Feature::repeated_string_values, v);
    }

  } else {
    for (auto const& kv : f.meta_) {
      pb.add_string(tags::Feature::repeated_string_keys, kv.first);
    }
    for (auto const& kv : f.meta_) {
      pb.add_string(tags::Feature::repeated_string_values, kv.second);
    }
  }

  if (!fast) {
    for (auto const& mask : make_simplify_mask(f.geometry_)) {
      pb.add_string(tags::Feature::repeated_string_simplify_masks, mask);
    }
  }

  pb.add_message(tags::Feature::required_FixedGeometry_geometry,
                 serialize(f.geometry_));

  return buf;
}

}  // namespace tiles
