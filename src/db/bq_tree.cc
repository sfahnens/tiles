#include "tiles/db/bq_tree.h"

#include <array>
#include <map>
#include <stack>

#include "geo/tile.h"

#include "tiles/util.h"

/**
bq_tree is a binary quad tree following the tile pyramid
 - the root of the tree is tile (0, 0, 0)
 - each leaf is associated with one one bit of data
 - each node stores the values of its children (enumerated 0-3)
 - each node is represented as 32 bit integer containing three parts
   - first 4 bits: 1 if child is a TRUE leaf
   - next 4 bits: 1 if child is a FALSE leaf
   - other bits: base offset (from start of vector) to the first child node
 - only those child nodes exist which are not maked as TRUE or FALSE in parent
*/

namespace tiles {

constexpr auto const kBQBits = sizeof(bq_node_t) * 8;

constexpr auto const kTrueOffset = kBQBits - 4;
constexpr auto const kFalseOffset = kBQBits - 8;

constexpr auto const kOffsetMask =
    (static_cast<bq_node_t>(1) << (kBQBits - 8)) - 1;

constexpr auto const kEmptyRoot = std::numeric_limits<bq_node_t>::min();
constexpr auto const kFullRoot = std::numeric_limits<bq_node_t>::max();

uint32_t quad_pos(geo::tile const& tile) {
  return (tile.x_ % 2 << 1) | (tile.y_ % 2);
}

bool bit_set(uint32_t val, uint32_t idx) { return (val & (1 << idx)) != 0; }

bq_tree::bq_tree() : nodes_{kEmptyRoot} {}

bool bq_tree::contains(geo::tile const& query) const {
  if (nodes_.at(0) == kFullRoot) {
    return true;
  } else if (nodes_.at(0) == kEmptyRoot || query == geo::tile{0, 0, 0}) {
    return false;
  }

  std::vector<geo::tile> trace{query};
  while (!(trace.back().parent() == geo::tile{0, 0, 0})) {
    trace.push_back(trace.back().parent());
  }
  std::reverse(begin(trace), end(trace));

  // curr is at lvl z, tile is at lvl z+1
  auto curr = nodes_.at(0);
  for (auto const& tile : trace) {
    // std::cout << "tile" << tile << std::endl;
    // printf("%i %08x\n\n", quad_pos(tile), curr);

    if (bit_set(curr, quad_pos(tile) + kFalseOffset)) {
      return false;
    }
    if (bit_set(curr, quad_pos(tile) + kTrueOffset)) {
      return true;
    }

    auto offset = curr & kOffsetMask;
    for (auto i = 0u; i < quad_pos(tile); ++i) {
      if (!bit_set(curr, i + kFalseOffset) && !bit_set(curr, i + kTrueOffset)) {
        ++offset;
      }
    }

    curr = nodes_.at(offset);
  }

  return false;
}

std::string_view bq_tree::string_view() const {
  return std::string_view{reinterpret_cast<char const*>(nodes_.data()),
                          nodes_.size() * sizeof(bq_node_t)};
}

void bq_tree::dump() const {
  std::cout << "bq_tree with " << nodes_.size() << " nodes:\n";
  for (auto i = 0u; i < nodes_.size(); ++i) {
    printf("%i | %08x\n", i, nodes_[i]);
  }
  std::cout << "---" << std::endl;
}

struct bq_tmp_node_t {
  bool leaf_;
  std::array<bq_tmp_node_t const*, 4> children_;
};

bq_tree serialize_bq_tree(bq_tmp_node_t const& root) {
  std::stack<std::pair<size_t, bq_tmp_node_t const*>> stack;
  stack.emplace(0, &root);

  std::vector<bq_node_t> vec;
  vec.emplace_back(0);

  while (!stack.empty()) {
    auto const[offset, node] = stack.top();  // copy required!
    stack.pop();

    for (auto i = 0u; i < 4u; ++i) {
      auto& storage = vec.at(offset);  // emplace_back invalidates maybe!
      auto const* child = node->children_[i];

      if (child == nullptr) {
        storage |= 1 << (i + kFalseOffset);
      } else if (child->leaf_) {
        storage |= 1 << (i + kTrueOffset);
      } else {
        if ((storage & kOffsetMask) == 0) {
          storage |= vec.size();
        }

        stack.emplace(vec.size(), child);
        vec.emplace_back(0);
      }
    }
  }
  return bq_tree{std::move(vec)};
}

bq_tree make_bq_tree(std::vector<geo::tile> const& tiles) {
  if (tiles.empty()) {
    return bq_tree{{kEmptyRoot}};
  }

  std::vector<std::map<geo::tile, bq_tmp_node_t>> nodes{{}};
  for (auto const& tile : tiles) {
    if (tile.z_ + 1 > nodes.size()) {
      nodes.resize(tile.z_ + 1);
    }
    nodes[tile.z_][tile] = {true, {nullptr, nullptr, nullptr, nullptr}};
  }

  for (auto z = nodes.size() - 1; z > 0; --z) {
    for (auto const& pair : nodes[z]) {
      auto const& tile = pair.first;
      auto& parent = nodes[z - 1][tile.parent()];
      if (!parent.leaf_) {
        parent.children_[quad_pos(tile)] = &pair.second;
      }
    }
  }

  // aggregate full nodes to leafs
  verify(nodes.at(0).size() == 1, "root node missing");
  auto const& root = begin(nodes.at(0))->second;
  if (root.leaf_) {
    return bq_tree{{kFullRoot}};
  }
  return serialize_bq_tree(root);
}

}  // namespace tiles
