#pragma once

#include "protozero/pbf_builder.hpp"

#include "tiles/feature/feature.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/algo/delta.h"
#include "tiles/fixed/algo/make_simplify_mask.h"
#include "tiles/fixed/io/serialize.h"

namespace tiles {

inline std::string serialize_feature(feature const& f) {
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

  for (auto const & [ k, _ ] : f.meta_) {
    pb.add_string(tags::Feature::repeated_string_keys, k);
  }
  for (auto const & [ _, v ] : f.meta_) {
    pb.add_string(tags::Feature::repeated_string_values, v);
  }

  for (auto const& mask : make_simplify_mask(f.geometry_)) {
    pb.add_string(tags::Feature::repeated_string_simplify_masks, mask);
  }

  pb.add_message(tags::Feature::required_FixedGeometry_geometry,
                 serialize(f.geometry_));

  return buf;
}

}  // namespace tiles
