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

  // TODO check first is not last!

  for (auto const& item : area) {
    switch (item.type()) {
      case osmium::item_type::outer_ring:
        polygon.emplace_back();
        nodes_to_fixed(*reinterpret_cast<osmium::OuterRing const*>(item.data()),
                       polygon.back().outer());
        if (polygon.back().outer().size() < 3) {
          return fixed_null{};
        }

        break;
      case osmium::item_type::inner_ring:
        utl::verify(!polygon.empty(), "inner ring first!");

        polygon.back().inners().emplace_back();
        nodes_to_fixed(*reinterpret_cast<osmium::InnerRing const*>(item.data()),
                       polygon.back().inners().back());

        if (polygon.back().inners().back().size() < 3) {
          polygon.back().inners().back().pop_back();
        }

        break;
      default: break;
    }
  }

  if (polygon.empty()) {
    return fixed_null{};
  }

  return polygon;
}

}  // namespace tiles
