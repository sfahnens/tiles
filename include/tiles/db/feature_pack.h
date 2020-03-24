#pragma once

#include <string>
#include <vector>

#include "protozero/varint.hpp"

#include "tiles/bin_utils.h"
#include "tiles/db/quad_tree.h"

// feature pack "wire format" specification
//
//  1b : header?! TODO implement this
//  4b : feature count uint32_t
//  4b : offset of index
// var : payload [serialized_feature | \0]
// var : quad_trees per level quad trees
// var : index to the quad trees

namespace tiles {

struct tile_db_handle;
struct pack_handle;
struct shared_metadata_coder;

// quick packing (e.g. as part of a insert flush)
std::string pack_features(std::vector<std::string> const&);

// optimal packing (incl. index)
std::string pack_features(geo::tile const&, shared_metadata_coder const&,
                          std::vector<std::string_view> const&);

// full database packing (e.g. once and optimal)
void pack_features(tile_db_handle&, pack_handle&);

template <typename Fn>
void unpack_features(std::string_view const& string, Fn&& fn) {
  utl::verify(string.size() > 8, "invalid feature_pack");
  auto const feature_count = read_nth<uint32_t>(string.data(), 0);

  auto ptr = string.data() + 2 * sizeof(uint32_t);
  auto const end = string.data() + string.size();
  for (auto i = 0u; i < feature_count; ++i) {
    uint64_t size = 0;
    while (size == 0) {  // skip zero elements (= span terminators)
      size = protozero::decode_varint(&ptr, end);
    }
    fn(std::string_view{ptr, size});
    ptr += size;
  }
}

template <typename Fn>
void unpack_features(geo::tile const& root, std::string_view const& string,
                     geo::tile const& tile, Fn&& fn) {
  utl::verify(string.size() > 8, "invalid feature_pack");
  auto const idx_offset = read_nth<uint32_t>(string.data(), 1);

  if (idx_offset == 0) {
    return unpack_features(string, fn);  // no tree available
  }

  utl::verify(string.size() >= idx_offset, "invalid feature_pack idx_offset");
  auto idx_ptr = string.data() + idx_offset;
  auto const end = string.data() + string.size();
  for (auto z = root.z_; z <= std::max(root.z_, tile.z_); ++z) {
    auto const tree_offset = protozero::decode_varint(&idx_ptr, end);
    if (tree_offset == 0) {
      continue;  // index empty
    }

    walk_quad_tree(
        string.data() + tree_offset, root, tile,
        [&](auto const span_offset, auto const span_count) {
          auto span_ptr = string.data() + span_offset;
          for (auto i = 0u; i < span_count; ++i) {
            uint64_t size;
            while ((size = protozero::decode_varint(&span_ptr, end)) != 0) {
              fn(std::string_view{span_ptr, size});
              span_ptr += size;
            }
          }
        });
  }
}

}  // namespace tiles
