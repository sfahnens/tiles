#include "tiles/db/feature_pack.h"

#include "utl/equal_ranges.h"
#include "utl/to_vec.h"

#include "tiles/db/quad_tree.h"
#include "tiles/feature/deserialize.h"
#include "tiles/feature/feature.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/mvt/tile_spec.h"

namespace tiles {

struct packer {
  explicit packer(uint32_t feature_count) {
    tiles::append<uint32_t>(buf_, feature_count);  // write feature count
    tiles::append<uint32_t>(buf_, 0u);  // reserve space for the index ptr
  }

  void write_index_offset(uint32_t const offset) {
    write_nth<uint32_t>(buf_.data(), 1, offset);
  }

  template <typename It>
  uint32_t append_span(It begin, It end) {
    uint32_t offset = buf_.size();

    for (auto it = begin; it != end; ++it) {
      verify(it->size() >= 32, "MINI FEATURE?!");
      protozero::write_varint(std::back_inserter(buf_), it->size());
      buf_.append(it->data(), it->size());
    }
    // null terminated list
    protozero::write_varint(std::back_inserter(buf_), 0ul);

    return offset;
  }

  uint32_t append_packed(std::vector<uint32_t> const& vec) {
    uint32_t offset = buf_.size();
    for (auto const& e : vec) {
      protozero::write_varint(std::back_inserter(buf_), e);
    }
    return offset;
  }

  template <typename String>
  uint32_t append(String const& string) {
    uint32_t offset = buf_.size();
    buf_.append(string);
    return offset;
  }

  std::string buf_;
};

std::string pack_features(std::vector<std::string> const& strings) {
  packer p{static_cast<uint32_t>(strings.size())};
  p.append_span(begin(strings), end(strings));
  return p.buf_;
}

geo::tile find_best_tile(geo::tile const& root, feature const& feature) {
  auto const& feature_box = bounding_box(feature.geometry_);

  geo::tile best = root;
  while (best.z_ <= kMaxZoomLevel) {
    std::optional<geo::tile> next_best;
    for (auto const& child : best.direct_children()) {
      auto const tile_box = tile_spec{child}.draw_bounds_;
      if ((feature_box.max_corner().x() < tile_box.min_corner().x() ||
           feature_box.min_corner().x() > tile_box.max_corner().x()) ||
          (feature_box.max_corner().y() < tile_box.min_corner().y() ||
           feature_box.min_corner().y() > tile_box.max_corner().y())) {
        continue;
      }
      if (next_best) {
        return best;  // two matches -> take prev best
      }
      next_best = child;
    }

    verify(next_best, "at least one child must match");
    best = *next_best;
  }

  return best;
}

std::vector<uint8_t> make_quad_key(geo::tile const& root,
                                   geo::tile const& tile) {
  if (tile == root) {
    return {};
  }

  std::vector<geo::tile> trace{tile};
  while (!(trace.back().parent() == root)) {
    verify(trace.back().z_ > root.z_, "tile outside root");
    trace.push_back(trace.back().parent());
  }
  trace.push_back(root);
  std::reverse(begin(trace), end(trace));

  return utl::to_vec(trace,
                     [](auto const& t) -> uint8_t { return t.quad_pos(); });
}

struct packable_feature {
  packable_feature(std::vector<uint8_t> quad_key, geo::tile best_tile,
                   std::string const* feature)
      : quad_key_{std::move(quad_key)},
        best_tile_{std::move(best_tile)},
        feature_{feature} {}

  friend bool operator<(packable_feature const& a, packable_feature const& b) {
    return std::tie(a.quad_key_, a.best_tile_, a.feature_) <
           std::tie(b.quad_key_, b.best_tile_, b.feature_);
  }

  char const* data() const { return feature_->data(); }
  size_t size() const { return feature_->size(); }

  std::vector<uint8_t> quad_key_;
  geo::tile best_tile_;
  std::string const* feature_;
};

std::string pack_features(geo::tile const& tile,
                          std::vector<std::string> const& strings) {

  std::vector<std::vector<packable_feature>> features_by_min_z(kMaxZoomLevel +
                                                               1 - tile.z_);
  for (auto const& str : strings) {
    auto const feature = deserialize_feature(str);
    verify(feature, "feature must be valid (!?)");

    auto const best_tile = find_best_tile(tile, *feature);
    auto const z = std::max(tile.z_, feature->zoom_levels_.first) - tile.z_;
    features_by_min_z.at(z).emplace_back(make_quad_key(tile, best_tile),
                                         best_tile, &str);
  }

  packer p{static_cast<uint32_t>(strings.size())};

  std::vector<std::string> quad_trees;
  for (auto& features : features_by_min_z) {
    if (features.empty()) {
      quad_trees.emplace_back();
      continue;
    }

    std::vector<quad_tree_input> quad_tree_input;
    utl::equal_ranges(
        features,
        [](auto const& a, auto const& b) { return a.quad_key_ < b.quad_key_; },
        [&](auto const& lb, auto const& ub) {
          quad_tree_input.push_back(
              {lb->best_tile_, p.append_span(lb, ub), 1u});
        });

    quad_trees.emplace_back(make_quad_tree(tile, quad_tree_input));
  }
  p.write_index_offset(
      p.append_packed(utl::to_vec(quad_trees, [&](auto const& quad_tree) {
        return quad_tree.empty() ? 0u : p.append(quad_tree);
      })));
  return p.buf_;
}

void pack_features(tile_db_handle& handle) {
  lmdb::txn txn{handle.env_};
  auto feature_dbi = handle.features_dbi(txn);
  lmdb::cursor c{txn, feature_dbi};

  geo::tile tile{std::numeric_limits<uint32_t>::max(),
                 std::numeric_limits<uint32_t>::max(),
                 std::numeric_limits<uint32_t>::max()};
  std::vector<std::string> features;

  for (auto el = c.get<tile_index_t>(lmdb::cursor_op::FIRST); el;
       el = c.get<tile_index_t>(lmdb::cursor_op::NEXT)) {

    auto const& this_tile = feature_key_to_tile(el->first);
    std::vector<std::string> this_features;
    unpack_features(el->second, [&this_features](auto const& view) {
      this_features.emplace_back(view);
    });

    c.del();

    if (!(tile == this_tile)) {
      if (!features.empty()) {
        std::cout << "PACK TILE: " << tile << std::endl;
        c.put(make_feature_key(tile), pack_features(tile, features));
      }

      tile = this_tile;
      features = std::move(this_features);
    } else {
      features.insert(end(features),
                      std::make_move_iterator(begin(this_features)),
                      std::make_move_iterator(end(this_features)));
    }
  }
  c.put(make_feature_key(tile), pack_features(features));

  txn.commit();
}

}  // namespace tiles
