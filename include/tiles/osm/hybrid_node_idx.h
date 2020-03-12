#pragma once

#include <memory>
#include <optional>

#include "osmium/handler.hpp"
#include "osmium/memory/buffer.hpp"
#include "osmium/osm/node.hpp"
#include "osmium/osm/types.hpp"

#include "tiles/fixed/convert.h"
#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

struct hybrid_node_idx : public osmium::handler::Handler {

  static constexpr auto x_offset = 180 * osmium::detail::coordinate_precision;
  static constexpr auto y_offset = 90 * osmium::detail::coordinate_precision;

  hybrid_node_idx();
  hybrid_node_idx(int idx_fd, int dat_fd);
  ~hybrid_node_idx();

  void way(osmium::Way& way) const;

  struct impl;
  std::unique_ptr<impl> impl_;
};

// TODO not real fixed_xy here
std::optional<fixed_xy> get_coords(hybrid_node_idx const&,
                                   osmium::object_id_type const&);

void get_coords(
    hybrid_node_idx const&,
    std::vector<std::pair<osmium::object_id_type, osmium::Location*>>&);

void update_locations(hybrid_node_idx const&, osmium::memory::Buffer&);

struct hybrid_node_idx_builder : public osmium::handler::Handler {
  hybrid_node_idx_builder(hybrid_node_idx&);
  hybrid_node_idx_builder(int idx_fd, int dat_fd);
  ~hybrid_node_idx_builder();

  void node(osmium::Node const& n) {
    push(n.id(), {n.location().x() + hybrid_node_idx::x_offset,
                  n.location().y() + hybrid_node_idx::y_offset});
  }

  void push(osmium::object_id_type const, fixed_xy const&);
  void finish();

  void dump_stats() const;
  size_t get_stat_spans() const;  // for tests

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
