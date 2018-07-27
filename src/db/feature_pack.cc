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
  packer() {
    append<uint32_t>(buf_, 0u);  // reserve space for the feature count
    append<uint32_t>(buf_, 0u);  // reserve space for the index ptr
    append<uint32_t>(buf_, 0u);  // reserve space for the quad tree ptr
  }

  void append_feature(std::string const& str) {
    buf_.append(str);
    offsets_.push_back(buf_.size());
  }

  void write_index() {
    write_nth<uint32_t>(buf_.data(), 0,
                        offsets_.size());  // write feature_count
    write_nth<uint32_t>(buf_.data(), 1, buf_.size());  // write index ptr
    for (auto const& e : offsets_) {
      append<uint32_t>(buf_, e);
    }
  }

  std::string buf_;
  std::vector<uint32_t> offsets_;
};

std::string pack_features(std::vector<std::string> const& strings) {
  packer p;
  for (auto const& str : strings) {
    p.append_feature(str);
  }
  p.write_index();
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

std::string pack_features(geo::tile const& tile,
                          std::vector<std::string> const& strings) {
  packer p;

  std::map<uint32_t, size_t> best_z;

  auto tmp = utl::to_vec(strings, [&](auto const& str) {
    auto const feature = deserialize_feature(str);
    verify(feature, "feature must be valid (!?)");

    auto const best_tile = find_best_tile(tile, *feature);
    ++best_z[best_tile.z_];

    auto quad_key = make_quad_key(tile, best_tile);
    return std::make_tuple(quad_key, best_tile, &str);
  });

  std::vector<quad_tree_input> quad_tree_input;
  utl::equal_ranges(tmp,
                    [](auto const& lhs, auto const& rhs) {
                      return std::get<0>(lhs) < std::get<0>(rhs);
                    },
                    [&](auto const& lb, auto const& ub) {
                      for (auto it = lb; it != ub; ++it) {
                        p.append_feature(*std::get<2>(*it));
                      }

                      quad_tree_input.push_back(
                          {std::get<1>(*lb),
                           static_cast<uint32_t>(std::distance(begin(tmp), lb)),
                           static_cast<uint32_t>(std::distance(lb, ub))});
                    });
  p.write_index();

  write_nth<uint32_t>(p.buf_.data(), 2, p.buf_.size());  // quad tree ptr
  p.buf_.append(make_quad_tree(tile, quad_tree_input));

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
