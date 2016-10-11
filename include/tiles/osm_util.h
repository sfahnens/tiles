#pragma once

#include "osmium/handler.hpp"
#include "osmium/io/pbf_input.hpp"
#include "osmium/io/reader_iterator.hpp"
#include "osmium/memory/buffer.hpp"
#include "osmium/osm.hpp"
#include "osmium/visitor.hpp"

namespace tiles {

template <typename F>
void foreach_osm_node(std::string const& filename, F f) {
  osmium::io::Reader reader(filename, osmium::osm_entity_bits::node);
  for (auto it = std::begin(reader); it != std::end(reader); ++it) {
    f(static_cast<osmium::Node&>(*it));
  }
}

template <typename F>
void foreach_osm_way(std::string const& filename, F f) {
  osmium::io::Reader reader(filename, osmium::osm_entity_bits::way);
  for (auto it = std::begin(reader); it != std::end(reader); ++it) {
    f(static_cast<osmium::Way&>(*it));
  }
}

template <typename F>
void foreach_osm_relation(std::string const& filename, F f) {
  osmium::io::Reader reader(filename, osmium::osm_entity_bits::relation);
  for (auto it = std::begin(reader); it != std::end(reader); ++it) {
    f(static_cast<osmium::Relation&>(*it));
  }
}

}  // namespace tiles
