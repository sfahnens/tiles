#include "tiles/loader/convert_osm_geometry.h"

#include "osmium/osm.hpp"

#include "utl/to_vec.h"

#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/io/serialize.h"

namespace tiles {

std::pair<fixed_box, std::string> convert_osm_geometry(
    osmium::Node const& node) {
  auto const loc =
      latlng_to_fixed({node.location().lat(), node.location().lon()});

  return {bounding_box(loc), serialize(loc)};
}

template <typename Container>
std::vector<fixed_xy> nodes_to_fixed(Container const& c) {
  return utl::to_vec(c, [](auto const& n) {
    return latlng_to_fixed({n.lat(), n.lon()});
  });
}

std::pair<fixed_box, std::string> convert_osm_geometry(osmium::Way const& way) {
  // TODO verify that distances fit into int32_t (or clipping will not
  // work)
  fixed_polyline polyline;
  polyline.geometry_.emplace_back(nodes_to_fixed(way.nodes()));

  return {bounding_box(polyline), serialize(polyline)};
}

std::pair<fixed_box, std::string> convert_osm_geometry(
    osmium::Area const& area) {
  fixed_polygon polygon;

  // TODO check first is not last!

  for (auto const& item : area) {
    switch (item.type()) {
      case osmium::item_type::outer_ring:
        polygon.type_.emplace_back(true);
        polygon.geometry_.emplace_back(nodes_to_fixed(
            *reinterpret_cast<osmium::OuterRing const*>(item.data())));
        break;
      case osmium::item_type::inner_ring:
        polygon.type_.emplace_back(false);
        polygon.geometry_.emplace_back(nodes_to_fixed(
            *reinterpret_cast<osmium::InnerRing const*>(item.data())));
      default: break;
    }
  }

  return {bounding_box(polygon), serialize(polygon)};
}

}  // namespace tiles
