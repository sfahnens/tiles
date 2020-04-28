#include "tiles/osm/read_osm_geometry.h"

#include "osmium/osm.hpp"

#include "utl/to_vec.h"
#include "utl/verify.h"

#include "tiles/fixed/convert.h"
#include "tiles/util.h"

namespace tiles {

fixed_geometry read_osm_geometry(osmium::Node const& node) {
  if (!node.location().valid()) {
    return fixed_null{};
  } else {
    return fixed_point{
        {latlng_to_fixed({node.location().lat_without_check(),
                          node.location().lon_without_check()})}};
  }
}

template <typename In, typename Out>
void nodes_to_fixed(In const& in, Out& out) {
  out.reserve(in.size());

  for (auto const& node : in) {
    if (node.location().valid()) {
      out.emplace_back(latlng_to_fixed({node.location().lat_without_check(),
                                        node.location().lon_without_check()}));
    }
  }
}

fixed_geometry read_osm_geometry(osmium::Way const& way) {
  // TODO utl::verify( that distances fit into int32_t (or clipping will not
  // work)
  fixed_polyline polyline;
  polyline.emplace_back();
  nodes_to_fixed(way.nodes(), polyline.back());

  if (polyline.back().size() < 2) {
    return fixed_null{};
  } else {
    return polyline;
  }
}

fixed_geometry read_osm_geometry(osmium::Area const& area) {
  fixed_polygon polygon;
  for (auto const& item : area) {
    switch (item.type()) {
      case osmium::item_type::outer_ring: {
        auto const& osm_r =
            *reinterpret_cast<osmium::OuterRing const*>(item.data());

        auto& fixed_r = polygon.emplace_back().outer();
        nodes_to_fixed(osm_r, fixed_r);

        if (!fixed_r.empty() && !(fixed_r.front() == fixed_r.back())) {
          fixed_r.push_back(fixed_r.front());
        }

        if (fixed_r.size() < 3) {
          return fixed_null{};
        }

      } break;
      case osmium::item_type::inner_ring: {
        utl::verify(!polygon.empty(), "inner ring first!");
        auto const& osm_r =
            *reinterpret_cast<osmium::InnerRing const*>(item.data());

        auto& fixed_r = polygon.back().inners().emplace_back();
        nodes_to_fixed(osm_r, fixed_r);

        if (!fixed_r.empty() && !(fixed_r.front() == fixed_r.back())) {
          fixed_r.push_back(fixed_r.front());
        }

        if (fixed_r.size() < 3) {
          polygon.back().inners().back().pop_back();
        }

      } break;
      default: break;
    }
  }

  if (polygon.empty()) {
    return fixed_null{};
  }

  return polygon;
}

}  // namespace tiles
