#include <iostream>

#include "tiles/osm/load_osm.h"

int main() {
  tiles::load_osm();
  std::cout << "import done!" << std::endl;
}
