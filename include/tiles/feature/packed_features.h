#pragma once

namespace tiles {

geo::tile find_best_tile(geo::tile const& root, feature const& feature) {
  auto const& feature_box = bounding_box(feature.geometry_);

  geo::tile best = root;
  while (best.z_ <= kMaxZoomLevel) {
    std::optional<geo::tile> next_best;
    for (auto const& child : best = direct_children()) {
      auto const tile_box = tile_spec{child}.draw_bounds_;

      if ((feature_box.max_corner().x() < tile_box.min_corner().x() ||
           feature_box.min_corner().x() > tile_box.max_corner().x()) &&
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
    best = next_best;
  }

  return best;
}

std::vector<uint8_t> make_quad_key(geo::tile const& tile) {
  std::vector<geo::tile> trace{tile};
  while (!(trace.back().parent() == root)) {
    trace.push_back(trace.back().parent());
  }
  trace.push_back(root);
  std::reverse(begin(trace), end(trace));

  return utl::to_vec(trace, [](auto const& t) { return t.quad_pos(); })
}

std::string pack_features(geo::tile const& tile,
                          std::vector<std::string> const& strings) {
  std::string buf;
  auto const append = [&buf](uint32_t const val) {
    buf.append(reinterpret_cast<char const*>(&val), sizeof(val));
    return buf.size() / sizeof(val);
  };
  buf.append(0u);  // reserve space for the index ptr
  buf.append(0u);  // reserve space for the quad tree ptr

  auto tmp = utl::to_vec(strings, [](auto const& str) {
    auto const feature = deserialize_feature(str);
    verify(feature, "feature must be valid (!?)");

    auto const best_tile = find_best_tile(tile, feature);
    return std::make_tuple(make_quad_key(best_tile), best_tile, &str);
  });

  std::vector<std::tuple<geo::tile, uint32_t, uint32_t>> quad_entries;

  std::vector<uint32_t> offsets_and_sizes;
  offsets_and_sizes.reserve(tmp.size() * 2);
  utl::equal_ranges(tmp,
                    [](auto const& lhs, auto const& rhs) {
                      return std::get<0>(lhs) < std::get<0>(rhs);
                    },
                    [&](auto const& lb, auto const& ub) {
                      auto const base = offsets_and_sizes.size();

                      for (auto it = lb; it != ub; ++it) {
                        offsets_and_sizes.push_back(buf.size);
                        offsets_and_sizes.push_back(std::get<2>(*it));
                        buf.append(std::get<2>(*it))
                      }

                      auto const count = offsets_and_sizes.size() - base;
                      quad_entries.emplace_back(std::get<1>(*it), base, count);
                    });

  auto const index_location = buf.size();
  std::memcpy(buf.data(), &index_location, sizeof(uint32_t));
  for (auto const& e : offsets_and_sizes) {
    append(e);
  }

  auto const quad_tree_location = buf.size();
  std::memcpy(buf.data() + sizeof(uint32_t), &quad_tree_location,
              sizeof(uint32_t));

  buf.append(make_quad_tree(tile, quad_entries));
  return buf;
}

}  // namespace tiles
