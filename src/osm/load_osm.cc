#include "tiles/osm/load_osm.h"

// For assembling multipolygons
#include "osmium/area/assembler.hpp"
#include "osmium/area/multipolygon_manager.hpp"

// For the NodeLocationForWays handler
#include "osmium/handler/node_locations_for_ways.hpp"

// Allow any format of input files (XML, PBF, ...)
#include "osmium/io/pbf_input.hpp"

// For osmium::apply()
#include "osmium/visitor.hpp"

// For the location index. There are different types of indexes available.
// This will work for all input files keeping the index in memory.
#include "osmium/index/map/flex_mem.hpp"

#include "osmium/util/progress_bar.hpp"

#include "tiles/osm/feature_handler.h"
#include "tiles/db/tile_database.h"

namespace tiles {

namespace o = osmium;
namespace oa = osmium::area;
namespace oio = osmium::io;
namespace oh = osmium::handler;
namespace orel = osmium::relations;
namespace ou = osmium::util;

using index_t =
    osmium::index::map::FlexMem<o::unsigned_object_id_type, o::Location>;
using location_handler_t = oh::NodeLocationsForWays<index_t>;

void load_osm() {

  tile_database db;

  // auto db = make_tile_database("database", false, false, {});
  // db->prepare_tiles(kMaxZoomLevel);
  feature_handler handler{db};

  // oio::File input_file{"/data/osm/hessen-latest.osm.pbf"};
  oio::File input_file{"/data/osm/2017-10-29/hessen-171029.osm.pbf"};

  oa::Assembler::config_type assembler_config;
  oa::MultipolygonManager<oa::Assembler> mp_manager{assembler_config};

  std::cerr << "Pass 1...\n";
  orel::read_relations(input_file, mp_manager);
  std::cerr << "Pass 1 done\n";

  std::cerr << "Memory:\n";
  orel::print_used_memory(std::cerr, mp_manager.used_memory());

  index_t index;
  location_handler_t location_handler{index};
  location_handler.ignore_errors();

  // On the second pass we read all objects and run them first through the
  // node location handler and then the multipolygon collector. The collector
  // will put the areas it has created into the "buffer" which are then
  // fed through our "handler".
  std::cerr << "Pass 2...\n";
  oio::Reader reader{input_file};
  // o::ProgressBar progress{reader.file_size(), ou::isatty(2)};

  o::apply(reader, location_handler, handler,
           mp_manager.handler(
               [&handler](auto&& buffer) { o::apply(buffer, handler); }));
  reader.close();
  std::cerr << "Pass 2 done\n";

  // Output the amount of main memory used so far. All complete multipolygon
  // relations have been cleaned up.
  std::cerr << "Memory:\n";
  orel::print_used_memory(std::cerr, mp_manager.used_memory());

  // If there were multipolgyon relations in the input, but some of their
  // members are not in the input file (which often happens for extracts)
  // this will write the IDs of the incomplete relations to stderr.
  // std::vector<osmium::object_id_type> incomplete_relations_ids;
  // mp_manager.for_each_incomplete_relation([&](const orel::RelationHandle&
  // handle){
  //     incomplete_relations_ids.push_back(handle->id());
  // });
  // if (!incomplete_relations_ids.empty()) {
  //     std::cerr << "Warning! Some member ways missing for these multipolygon
  //     relations:";
  //     for (const auto id : incomplete_relations_ids) {
  //         std::cerr << " " << id;
  //     }
  //     std::cerr << "\n";
  // }

  // std::cout << handler.count_ << std::endl;
}

}  // namespace tiles
