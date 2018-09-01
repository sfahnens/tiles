#pragma once

#include <map>
#include <string>
#include <vector>

#include "protozero/pbf_reader.hpp"
#include "protozero/pbf_writer.hpp"

#include "utl/get_or_create_index.h"

#include "tiles/db/tile_database.h"

namespace tiles {

constexpr auto const kLayerCoastlineIdx = 0ull;
constexpr auto const kLayerCoastlineName = "coastline";

struct layer_names_builder {
  layer_names_builder() {
    layer_names_[kLayerCoastlineName] = kLayerCoastlineIdx;
  }

  size_t get_layer_idx(std::string const& name) {
    return utl::get_or_create_index(layer_names_, name);
  }

  void store(tile_db_handle& handle, lmdb::txn& txn) const {
    std::vector<std::string_view> sorted;
    sorted.resize(layer_names_.size());
    for (auto const & [ name, idx ] : layer_names_) {
      sorted.at(idx) = name;
    }

    std::string buf;
    protozero::pbf_writer writer{buf};
    for (auto const& name : sorted) {
      writer.add_string(0, name);
    }

    auto meta_dbi = handle.meta_dbi(txn);
    txn.put(meta_dbi, kMetaKeyLayerNames, buf);
  }

  std::map<std::string, size_t> layer_names_;
};

inline std::vector<std::string> get_layer_names(tile_db_handle& handle,
                                                lmdb::txn& txn) {
  auto meta_dbi = handle.meta_dbi(txn);
  auto const opt_names = txn.get(meta_dbi, kMetaKeyLayerNames);
  if (!opt_names) {
    return {{kLayerCoastlineName}};
  }

  std::vector<std::string> names;
  protozero::pbf_reader reader{*opt_names};
  while (reader.next()) {
    names.emplace_back(reader.get_string());
  }
  return names;
}

}  // namespace tiles
