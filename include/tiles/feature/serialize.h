#pragma once

#include "protozero/pbf_builder.hpp"

#include "tiles/feature/feature.h"
#include "tiles/fixed/io/serialize.h"


namespace tiles {

std::string serialize_feature(feature const& f) {
  std::string buf;
  protozero::pbf_builder<tags::Feature> pb(buf);

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