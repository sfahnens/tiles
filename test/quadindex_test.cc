#include "catch.hpp"

#include <array>
#include <map>
#include <stack>

#include "fmt/core.h"
#include "geo/tile.h"

#include "tiles/util.h"

namespace tiles {

using quad_entry_t = uint32_t;
constexpr auto const kQuadEntryBits = sizeof(quad_entry_t) * 8;
constexpr auto const kChildOffset = kQuadEntryBits - 4;
constexpr auto const kOffsetMask =
    (static_cast<quad_entry_t>(1) << (kQuadEntryBits - 4)) - 1;

inline uint32_t quad_pos(geo::tile const& tile) {
  return (tile.y_ % 2 << 1) | (tile.x_ % 2);
}

inline bool bit_set(uint32_t val, uint32_t idx) {
  return (val & (1 << idx)) != 0;
}

struct tmp_quad_node_t {
  std::array<tmp_quad_node_t const*, 4> children_;
  uint32_t offset_, size_self_, size_subtree_;  // payload range
};

using tmp_quad_tree_t = std::vector<std::map<geo::tile, tmp_quad_node_t>>;

tmp_quad_tree_t make_tmp_quad_tree(
    geo::tile const& root,
    std::vector<std::tuple<geo::tile, uint32_t, uint32_t>> const& entries) {
  auto root_z = root.z_;
  std::vector<std::map<geo::tile, tmp_quad_node_t>> nodes{{}};
  for (auto const& entry : entries) {
    auto const& tile = std::get<0>(entry);
    verify_silent(static_cast<int>(tile.z_) >= static_cast<int>(root_z),
                  "entry z less than root z");
    verify_silent(root_z != tile.z_ || root == tile, "another tile at root z");

    auto const rel_z = tile.z_ - root_z;
    if (rel_z + 1 > nodes.size()) {
      nodes.resize(rel_z + 1);
    }
    nodes[rel_z][tile] = {{nullptr, nullptr, nullptr, nullptr},
                          std::get<1>(entry),
                          std::get<2>(entry),
                          std::get<2>(entry)};
  }

  for (auto z = nodes.size() - 1; z > 0; --z) {
    for (auto const& pair : nodes[z]) {
      auto const& tile = pair.first;

      auto& parent = nodes[z - 1]
                         .insert({tile.parent(),
                                  {{nullptr, nullptr, nullptr, nullptr},
                                   std::numeric_limits<uint32_t>::max(),
                                   0u,
                                   0u}})
                         .first->second;

      parent.children_[quad_pos(tile)] = &pair.second;
      parent.offset_ = std::min(parent.offset_, pair.second.offset_);
      parent.size_subtree_ += pair.second.size_subtree_;
    }
  }

  verify_silent(nodes.at(0).size() == 1, "root node missing / not unique");
  verify_silent(begin(nodes.at(0))->first == root, "root node mismatch");
  return nodes;
}

std::vector<quad_entry_t> serialize_quad_tree(tmp_quad_node_t const root) {
  std::stack<std::pair<size_t, tmp_quad_node_t const*>> stack;
  std::vector<quad_entry_t> vec;

  auto const allocate_and_enqueue = [&](auto const* node) {
    stack.emplace(vec.size(), node);

    vec.emplace_back(0);  // allocate: child ptr
    vec.emplace_back(0);  // allocate: range/offset
    vec.emplace_back(0);  // allocate: range/size_self
    vec.emplace_back(0);  // allocate: range/size_subtree
  };

  allocate_and_enqueue(&root);
  while (!stack.empty()) {
    auto const[offset, node] = stack.top();  // copy required!
    stack.pop();

    vec.at(offset + 1) = node->offset_;  // set: range/offset
    vec.at(offset + 2) = node->size_self_;  // set: range/size_self
    vec.at(offset + 3) = node->size_subtree_;  // set: range/size_subtree

    for (auto i = 0u; i < 4u; ++i) {
      auto& storage = vec.at(offset);  // emplace_back invalidates maybe!
      auto const* child = node->children_[i];

      if (child != nullptr) {
        if (storage == 0) {
          storage = vec.size();  // set: child_ptr
        }

        storage |= 1 << (i + kChildOffset);
        allocate_and_enqueue(child);
      }
    }
  }
  return vec;
}

std::string make_quad_tree(
    geo::tile const& root,
    std::vector<std::tuple<geo::tile, uint32_t, uint32_t>> const& entries) {
  std::string buf;
  auto const append = [&buf](uint32_t const val) {
    buf.append(reinterpret_cast<char const*>(&val), sizeof(val));
    return buf.size() / sizeof(val);
  };

  if (entries.empty()) {
    append(0u);  // child_ptr
    append(0u);  // range/offset
    append(0u);  // range/size_self
    append(0u);  // range/size_subtree
    return buf;
  }

  auto const tmp_tree = make_tmp_quad_tree(root, entries);
  auto const tree_vec = serialize_quad_tree(begin(tmp_tree.at(0))->second);

  buf.reserve(tree_vec.size() * sizeof(quad_entry_t));
  for (auto const& entry : tree_vec) {
    append(entry);
  }

  return buf;
}

template <typename Fun>
void walk_quad_tree(char const* tree, geo::tile const& root,
                    geo::tile const& query, Fun&& fun) {
  auto const decode = [&tree](auto const offset) {
    quad_entry_t result;
    std::memcpy(&result, tree + offset * sizeof(quad_entry_t),
                sizeof(quad_entry_t));
    return result;
  };

  if (decode(3) == 0) {
    return;  // whole tree empty
  }

  {
    auto parent = root;
    while (true) {
      if (parent == query) {
        return fun(decode(1), decode(3));  // whole range
      }
      if (parent.z_ == 0) {
        break;
      }
      parent = parent.parent();
    }
  }

  std::vector<geo::tile> trace{query};
  while (!(trace.back().parent() == root)) {
    if (static_cast<int>(trace.back().z_) - 1 < 0 ||
        trace.back().z_ < root.z_) {
      return;
    }
    trace.push_back(trace.back().parent());
  }
  trace.push_back(root);

  std::reverse(begin(trace), end(trace));

  auto offset = 0;
  for (auto it = begin(trace); it != end(trace); ++it) {
    auto const& tile = *it;
    // std::cout << "tile" << tile << " offset " << offset << std::endl;
    // printf("%i %08x\n\n", quad_pos(tile), curr);

    if (tile == query) {
      return fun(decode(offset + 1), decode(offset + 3));  // emit subtree
    } else if (decode(offset + 2) != 0) {
      fun(decode(offset + 1), decode(offset + 2));  // emit self
    }

    if (decode(offset) == 0) {
      return;  // no more children
    }

    // descend
    verify(std::next(it) != end(trace), "should not happen");
    auto const& next_tile = *std::next(it);

    auto const curr = decode(offset);
    offset = curr & kOffsetMask;
    for (auto i = 0u; i < quad_pos(next_tile); ++i) {
      if (bit_set(curr, i + kChildOffset)) {
        offset += 4;  // four entries per node
      }
    }
  }
}

void dump_tree(std::string const& tree) {
  verify_silent(tree.size() % (4 * 4) == 0, "invalid tree size");

  auto const decode = [&tree](auto const offset) {
    quad_entry_t result;
    std::memcpy(&result, tree.data() + offset * sizeof(quad_entry_t),
                sizeof(quad_entry_t));
    return result;
  };

  for (auto i = 0u; i < tree.size() / 4; i += 4) {
    std::cout << fmt::format("{:4} {:032b} : {:10x} : {:5} : {:10} {:10} {:10}",
                             i, decode(i), decode(i), decode(i) & kOffsetMask,
                             decode(i + 1), decode(i + 2), decode(i + 3))
              << std::endl;
  }
}

}  // namespace tiles

std::vector<std::pair<uint32_t, uint32_t>> collect(char const* tree,
                                                   geo::tile const& root,
                                                   geo::tile const& tile) {
  std::vector<std::pair<uint32_t, uint32_t>> result;
  tiles::walk_quad_tree(tree, root, tile,
                        [&](auto const offset, auto const size) {
                          result.emplace_back(offset, size);
                        });
  std::sort(begin(result), end(result));
  return result;
}

TEST_CASE("quadindex") {
  SECTION("empty tree") {
    geo::tile root = {0, 0, 0};
    auto tree = tiles::make_quad_tree(root, {});
    REQUIRE(16 == tree.size());

    CHECK(true == collect(tree.data(), root, {0, 0, 0}).empty());
    CHECK(true == collect(tree.data(), root, {1, 1, 2}).empty());
  }

  SECTION("broken tree input") {
    geo::tile root{0, 0, 1};
    CHECK_THROWS(tiles::make_quad_tree(root, {{{0, 1, 1}, 0, 0}}));
    CHECK_THROWS(tiles::make_quad_tree(root, {{{0, 0, 0}, 0, 0}}));
    CHECK_THROWS(tiles::make_quad_tree(root, {{{2, 2, 2}, 0, 0}}));
  }

  SECTION("root tree") {
    geo::tile root{4, 5, 6};

    auto tree = tiles::make_quad_tree(root, {{root, 42, 23}});
    REQUIRE(16 == tree.size());

    // query outside
    CHECK(true == collect(tree.data(), root, {8, 8, 6}).empty());
    CHECK(true == collect(tree.data(), root, {8, 8, 5}).empty());

    // query above
    {
      auto content = collect(tree.data(), root, {0, 0, 2});
      REQUIRE(1 == content.size());
      CHECK(42 == content.at(0).first);
      CHECK(23 == content.at(0).second);
    }

    // query root
    {
      auto content = collect(tree.data(), root, root);
      REQUIRE(1 == content.size());
      CHECK(42 == content.at(0).first);
      CHECK(23 == content.at(0).second);
    }

    // query child
    {
      auto content = collect(tree.data(), root, {8, 10, 7});
      REQUIRE(1 == content.size());
      CHECK(42 == content.at(0).first);
      CHECK(23 == content.at(0).second);
    }
  }

  SECTION("child tree") {
    geo::tile root{0, 0, 1};

    auto tree = tiles::make_quad_tree(
        root, {{{0, 0, 2}, 1, 3}, {{0, 2, 4}, 5, 1}, {{0, 0, 4}, 4, 1}});

    // tiles::dump_tree(tree);

    {
      auto content = collect(tree.data(), root, {0, 0, 0});
      REQUIRE(1 == content.size());
      CHECK(1 == content.at(0).first);
      CHECK(5 == content.at(0).second);
    }
    {
      auto content = collect(tree.data(), root, {0, 0, 1});
      REQUIRE(1 == content.size());
      CHECK(1 == content.at(0).first);
      CHECK(5 == content.at(0).second);
    }
    {
      auto content = collect(tree.data(), root, {0, 0, 2});
      REQUIRE(1 == content.size());
      CHECK(1 == content.at(0).first);
      CHECK(5 == content.at(0).second);
    }
    {
      auto content = collect(tree.data(), root, {0, 0, 3});
      REQUIRE(2 == content.size());

      CHECK(1 == content.at(0).first);
      CHECK(3 == content.at(0).second);

      CHECK(4 == content.at(1).first);
      CHECK(1 == content.at(1).second);
    }
    {
      auto content = collect(tree.data(), root, {0, 2, 4});
      REQUIRE(2 == content.size());

      CHECK(1 == content.at(0).first);
      CHECK(3 == content.at(0).second);

      CHECK(5 == content.at(1).first);
      CHECK(1 == content.at(1).second);
    }
    {
      auto content = collect(tree.data(), root, {0, 4, 5});
      REQUIRE(2 == content.size());

      CHECK(1 == content.at(0).first);
      CHECK(3 == content.at(0).second);

      CHECK(5 == content.at(1).first);
      CHECK(1 == content.at(1).second);
    }
  }
}
