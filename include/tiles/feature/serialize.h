#pragma once

#include "protozero/pbf_builder.hpp"

#include "tiles/feature/feature.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/io/serialize.h"

namespace tiles {

inline std::string serialize_feature(feature const& f) {
  std::string buf;
  protozero::pbf_builder<tags::Feature> pb(buf);

  pb.add_uint32(tags::Feature::required_uint32_minzoomlevel,
                f.zoom_levels_.first);
  pb.add_uint32(tags::Feature::required_uint32_maxzoomlevel,
                f.zoom_levels_.second);

  auto const box = bounding_box(f.geometry_);
  auto const box_mask = (static_cast<uint64_t>(1) << 32) - 1;

  uint64_t box_x = (box.min_corner().x() & box_mask) |
                   ((box.max_corner().x() & box_mask) << 32);
  pb.add_uint64(tags::Feature::required_uint64_box_x, box_x);
  uint64_t box_y = (box.min_corner().y() & box_mask) |
                   ((box.max_corner().y() & box_mask) << 32);
  pb.add_uint64(tags::Feature::required_uint64_box_y, box_y);

  pb.add_uint64(tags::Feature::required_uint64_id, f.id_);

  for (auto const & [ k, _ ] : f.meta_) {
    pb.add_string(tags::Feature::repeated_string_keys, k);
  }
  for (auto const & [ _, v ] : f.meta_) {
    pb.add_string(tags::Feature::repeated_string_values, v);
  }

  pb.add_message(tags::Feature::required_FixedGeometry_geometry,
                 serialize(f.geometry_));

  return buf;
}

}  // namespace tiles
