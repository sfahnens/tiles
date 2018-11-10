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
#include "osmium/index/map/dense_mmap_array.hpp"
#include "osmium/index/map/flex_mem.hpp"

#include "osmium/util/progress_bar.hpp"

#include "tiles/db/tile_database.h"
#include "tiles/osm/feature_handler.h"
#include "tiles/osm/hybrid_node_idx.h"

namespace tiles {

namespace o = osmium;
namespace oa = osmium::area;
namespace oio = osmium::io;
namespace oh = osmium::handler;
namespace orel = osmium::relations;
namespace ou = osmium::util;
namespace oeb = osmium::osm_entity_bits;

using index_t =
    osmium::index::map::FlexMem<o::unsigned_object_id_type, o::Location>;
using location_handler_t = oh::NodeLocationsForWays<index_t>;

void load_osm(tile_db_handle& handle, std::string const& fname) {
  oio::File input_file{fname};

  oa::MultipolygonManager<oa::Assembler> mp_manager{
      oa::Assembler::config_type{}};

  hybrid_node_idx node_idx;

  {
    t_log("Pass 1...");
    hybrid_node_idx_builder node_idx_builder{node_idx};

    o::ProgressBar progress_bar{input_file.size(), ou::isatty(2)};
    oio::Reader reader{input_file, oeb::node | oeb::relation};
    while (auto buffer = reader.read()) {
      progress_bar.update(reader.offset());
      o::apply(buffer, node_idx_builder, mp_manager);
    }
    reader.close();
    progress_bar.file_done(input_file.size());
    t_log("Pass 1 done");

    mp_manager.prepare_for_lookup();
    t_log("Multipolygon Manager Memory:");
    orel::print_used_memory(std::cout, mp_manager.used_memory());

    node_idx_builder.finish();
    t_log("Hybrid Node Index Statistics:");
    node_idx_builder.dump_stats();
  }

  // index_t index;
  // location_handler_t location_handler{index};
  // location_handler.ignore_errors();

  feature_inserter inserter{handle, &tile_db_handle::features_dbi};
  layer_names_builder names_builder;

  feature_handler handler{inserter, names_builder};

  {
    t_log("Pass 2...");
    oio::Reader reader{input_file};
    o::ProgressBar progress{reader.file_size(), ou::isatty(2)};

    o::apply(reader, node_idx,  // location_handler,
             handler, mp_manager.handler([&handler](auto&& buffer) {
               o::apply(buffer, handler);
             }));
    reader.close();
    t_log("Pass 2 done");

    t_log("Multipolygon Manager Memory:");
    orel::print_used_memory(std::cout, mp_manager.used_memory());
  }

  names_builder.store(handle, inserter.txn_);
}

}  // namespace tiles
