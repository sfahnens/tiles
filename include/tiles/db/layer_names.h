#pragma once

#include <map>
#include <string>
#include <vector>

#include "protozero/pbf_reader.hpp"
#include "protozero/pbf_writer.hpp"

#include "utl/get_or_create_index.h"

#include "tiles/db/tile_database.h"

namespace tiles {

constexpr auto const kLayerCoastlineIdx = 0ULL;
constexpr auto const kLayerCoastlineName = "coastline";

template <typename Vec>
inline std::string write_layer_names(Vec&& vec) {
  std::string buf;
  protozero::pbf_writer writer{buf};
  for (auto const& name : vec) {
    writer.add_string(1, name);
  }
  return buf;
}

inline std::vector<std::string> read_layer_names(std::string_view const& str) {
  std::vector<std::string> vec;
  protozero::pbf_reader reader{str};
  while (reader.next()) {
    vec.emplace_back(reader.get_string());
  }
  return vec;
}

struct layer_names_builder {
  layer_names_builder() {
    layer_names_[kLayerCoastlineName] = kLayerCoastlineIdx;
  }

  size_t get_layer_idx(std::string const& name) {
    std::lock_guard<std::mutex> l{mutex_};
    return utl::get_or_create_index(layer_names_, name);
  }

  void store(tile_db_handle& handle, lmdb::txn& txn) const {
    std::vector<std::string_view> sorted;
    sorted.resize(layer_names_.size());
    for (auto const& [name, idx] : layer_names_) {
      sorted.at(idx) = name;
    }

    auto buf = write_layer_names(sorted);
    auto meta_dbi = handle.meta_dbi(txn);
    txn.put(meta_dbi, kMetaKeyLayerNames, buf);
  }

  std::mutex mutex_;
  std::map<std::string, size_t> layer_names_;
};

inline std::vector<std::string> get_layer_names(tile_db_handle& handle,
                                                lmdb::txn& txn) {
  auto meta_dbi = handle.meta_dbi(txn);
  auto const opt_names = txn.get(meta_dbi, kMetaKeyLayerNames);
  if (!opt_names) {
    return {{kLayerCoastlineName}};
  }

  return read_layer_names(*opt_names);
}

}  // namespace tiles
