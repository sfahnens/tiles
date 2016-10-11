#pragma once

#include <string>
#include <vector>

#include "geo/latlng.h"

#include "tiles/osm_util.h"

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

}  // namespace tiles