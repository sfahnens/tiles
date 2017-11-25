#pragma once

#include <utility>

#include "tiles/fixed/fixed_geometry.h"

namespace osmium {

class Node;
class Way;
class Area;

}  // namespace osmium

namespace tiles {

fixed_geometry read_osm_geometry(osmium::Node const&);
fixed_geometry read_osm_geometry(osmium::Way const&);
fixed_geometry read_osm_geometry(osmium::Area const&);

}  // namespace tiles
