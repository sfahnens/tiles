#pragma once

#include <memory>
#include <optional>

#include "osmium/osm/types.hpp"

#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

struct hybrid_node_idx {
  hybrid_node_idx();
  hybrid_node_idx(int idx_fd, int dat_fd);
  ~hybrid_node_idx();

  struct impl;
  std::unique_ptr<impl> impl_;
};

std::optional<fixed_xy> get_coords(hybrid_node_idx const&,
                                   osmium::object_id_type const&);

struct hybrid_node_idx_builder {
  hybrid_node_idx_builder(hybrid_node_idx&);
  hybrid_node_idx_builder(int idx_fd, int dat_fd);
  ~hybrid_node_idx_builder();

  void push(osmium::object_id_type const, fixed_xy const&);
  void finish();

  void dump_stats() const;
  size_t get_stat_spans() const; // for tests


  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
