#pragma once

#include <string>
#include <vector>

#include "tiles/bin_utils.h"
#include "tiles/db/tile_database.h"
#include "tiles/db/quad_tree.h"

namespace tiles {

// quick packing (e.g. as part of a insert flush)
std::string pack_features(std::vector<std::string> const&);

// full database packing (e.g. once)
void pack_features(tile_db_handle&);

template <typename Fn>
void unpack_features(std::string_view const& string, Fn&& fn) {
  auto const feature_count = read_nth<uint32_t>(string.data(), 0);
  auto const index_ptr = string.data() + read_nth<uint32_t>(string.data(), 1);

  auto offset = 3 * sizeof(uint32_t);
  for (auto i = 0u; i < feature_count; ++i) {
    auto const next_offset = read_nth<uint32_t>(index_ptr, i);
    fn(std::string_view{string.data() + offset, next_offset - offset});
    offset = next_offset;
  }
}

template <typename Fn>
void unpack_features(geo::tile const& root, std::string_view const& string,
                     geo::tile const& tile, Fn&& fn) {
  auto const index_ptr = string.data() + read_nth<uint32_t>(string.data(), 1);
  auto const tree_ptr = string.data() + read_nth<uint32_t>(string.data(), 2);

  if (tree_ptr == 0) {
    return unpack_features(string, fn);  // no tree available
  }

  walk_quad_tree(tree_ptr, root, tile, [&](auto const idx, auto const size) {
    auto offset = idx == 0 ? 3 * sizeof(uint32_t)
                           : read_nth<uint32_t>(index_ptr, idx - 1);
    for (auto i = 0u; i < size; ++i) {
      auto const next_offset = read_nth<uint32_t>(index_ptr, idx + i);
      fn(std::string_view{string.data() + offset, next_offset - offset});
      offset = next_offset;
    }
  });
}

}  // namespace tiles
