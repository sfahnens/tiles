#pragma once

#include <utility>

#include "tiles/fixed/fixed_geometry.h"

namespace osmium {

class Node;
class Way;
class Area;

}  // namespace osmium

namespace tiles {

std::pair<fixed_box, std::string> convert_osm_geometry(osmium::Node const&);
std::pair<fixed_box, std::string> convert_osm_geometry(osmium::Way const&);
std::pair<fixed_box, std::string> convert_osm_geometry(osmium::Area const&);

}  // namespace tiles
