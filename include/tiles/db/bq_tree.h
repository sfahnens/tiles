#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "geo/tile.h"

namespace tiles {

using bq_node_t = uint32_t;

struct bq_tree {
  bq_tree();
  explicit bq_tree(std::string_view);
  explicit bq_tree(std::vector<bq_node_t> nodes) : nodes_{std::move(nodes)} {}

  bool contains(geo::tile const& q) const;
  std::vector<geo::tile> all_leafs(geo::tile const& q) const;

  std::string_view string_view() const;
  void dump() const;

private:
  // first: if a parent leaf exists, the value of the leaf
  // second: if no parent leaf exists, the node of the queried tile
  std::pair<std::optional<bool>, bq_node_t> find_parent_leaf(
      geo::tile const& q) const;

public:
  std::vector<bq_node_t> nodes_;
};

bq_tree make_bq_tree(std::vector<geo::tile> const&);

}  // namespace tiles
