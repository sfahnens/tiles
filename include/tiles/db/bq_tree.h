#pragma once

#include <vector>

#include "geo/tile.h"

namespace tiles {

using bq_node_t = uint32_t;

struct bq_tree {
  bq_tree();
  explicit bq_tree(std::vector<bq_node_t> nodes) : nodes_{std::move(nodes)} {}

  bool contains(geo::tile const& query) const;

  std::string_view string_view() const;
  void dump() const;

  std::vector<bq_node_t> nodes_;
};

bq_tree make_bq_tree(std::vector<geo::tile> const&);

}  // namespace tiles
