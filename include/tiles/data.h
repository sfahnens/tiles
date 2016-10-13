#pragma once

#include <string>
#include <vector>

#include "geo/latlng.h"
#include "geo/polyline.h"

#include "tiles/osm_util.h"
#include "tiles/util.h"

namespace tiles {

struct city {
  std::string name_;
  geo::latlng pos_;
};

std::vector<city> load_cities(std::string const& osm_file) {
  std::string const city_str = "city";

  std::vector<city> cities;
  foreach_osm_node(osm_file, [&](auto const& node) {
    if (city_str != node.get_value_by_key("place", "")) {
      return;
    }

    cities.push_back({node.get_value_by_key("name", ""),
                      {node.location().lat(), node.location().lon()}});
  });

  return cities;
}

std::vector<geo::polyline> load_railways(std::string const& osm_file) {
  std::vector<std::unique_ptr<geo::latlng>> latlng_mem;
  std::vector<std::vector<geo::latlng*>> railways;
  std::map<int64_t, geo::latlng*> pending_latlngs;

  std::string rail{"rail"};
  std::string yes{"yes"};
  std::string crossover{"crossover"};
  std::vector<std::string> excluded_usages{"industrial", "military", "test",
                                           "tourism"};
  std::vector<std::string> excluded_services{"yard", "spur"};  // , "siding"

  foreach_osm_way(osm_file, [&](auto&& way) {
    if (rail != way.get_value_by_key("railway", "")) {
      return;
    }

    auto const usage = way.get_value_by_key("usage", "");
    if (std::any_of(begin(excluded_usages), end(excluded_usages),
                    [&](auto&& u) { return u == usage; })) {
      return;
    }

    auto const service = way.get_value_by_key("service", "");
    if (std::any_of(begin(excluded_services), end(excluded_services),
                    [&](auto&& s) { return s == service; })) {
      return;
    }

    if (yes == way.get_value_by_key("railway:preserved", "")) {
      return;
    }

    railways.emplace_back(transform_to_vec(
        way.nodes(), [&](auto const& node_ref) -> geo::latlng* {
          return get_or_create(pending_latlngs, node_ref.ref(), [&]() {
            latlng_mem.emplace_back(std::make_unique<geo::latlng>());
            return latlng_mem.back().get();
          });
        }));
  });

  foreach_osm_node(osm_file, [&](auto&& node) {
    auto it = pending_latlngs.find(node.id());
    if (it == end(pending_latlngs)) {
      return;
    }

    it->second->lat_ = node.location().lat();
    it->second->lng_ = node.location().lon();
  });

  return transform_to_vec(railways, [](auto const& railway) {
    return transform_to_vec(railway, [](auto const& ptr) { return *ptr; });
  });
}

}  // namespace tiles