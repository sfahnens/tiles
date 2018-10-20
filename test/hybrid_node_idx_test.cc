#include "catch.hpp"

#include <numeric>
#include <optional>
#include <random>

#include "osmium/index/detail/mmap_vector_file.hpp"
#include "osmium/index/detail/tmpfile.hpp"
#include "osmium/osm/types.hpp"

#include "osmium/io/pbf_input.hpp"
#include "osmium/io/reader_iterator.hpp"

#include "protozero/varint.hpp"

#include "utl/to_vec.h"

#include "tiles/constants.h"
#include "tiles/fixed/algo/delta.h"
#include "tiles/fixed/convert.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/util.h"

namespace pz = protozero;

namespace tiles {

using osm_id_t = osmium::object_id_type;

struct id_offset {
  id_offset(osm_id_t id, size_t offset) : id_{id}, offset_{offset} {}

  id_offset()
      : id_{std::numeric_limits<osm_id_t>::max()},
        offset_{std::numeric_limits<size_t>::max()} {}

  bool operator==(id_offset const& o) const {
    return std::tie(id_, offset_) == std::tie(o.id_, o.offset_);
  }

  osm_id_t id_;
  size_t offset_;
};

struct hybrid_node_idx {
  hybrid_node_idx(int idx_fd, int dat_fd) : idx_{idx_fd}, dat_{dat_fd} {
    auto const* it = dat_.data();

    auto const roots_size = pz::decode_varint(&it, std::end(dat_));
    verify(roots_size > 0, "roots must not be empty");
    for (auto i = 0u; i < roots_size; ++i) {
      roots_.emplace_back(pz::decode_varint(&it, std::end(dat_)),
                          pz::decode_varint(&it, std::end(dat_)));
    }

    root_idx_bits_ = make_root_idx_bits();
    t_log("reading {} roots ({} bits)", roots_.size(), root_idx_bits_);
  }

  hybrid_node_idx(int idx_fd, int dat_fd, std::vector<fixed_xy> roots)
      : idx_{idx_fd},
        dat_{dat_fd},
        roots_{std::move(roots)},
        root_idx_bits_{make_root_idx_bits()} {
    verify(!roots_.empty(), "roots must not be empty");
  }

  uint32_t make_root_idx_bits() const {
    return std::floor(std::log2(roots_.size()) + 1);
  }

  std::optional<fixed_xy> get_coords(osm_id_t const& id) const {
    if (idx_.empty()) {  // XXX very unlikely in prod
      return std::nullopt;
    }
    auto it = std::lower_bound(
        std::begin(idx_), std::end(idx_), id,
        [](auto const& o, auto const& i) { return o.id_ < i; });

    if (it == std::begin(idx_) && it->id_ != id) {  // not empty -> begin != end
      return std::nullopt;
    }
    if (it == std::end(idx_) || it->id_ != id) {
      --it;
    }

    osm_id_t curr_id = it->id_;
    auto dat_it = dat_.data() + it->offset_;

    while (curr_id <= id && dat_it != std::end(dat_)) {
      auto const header = pz::decode_varint(&dat_it, std::end(dat_));
      if (header == 0) {
        break;
      }

      auto const span_size = header >> root_idx_bits_;

      auto const root_idx_mask = (static_cast<size_t>(1) << root_idx_bits_) - 1;
      auto const root_idx = header & root_idx_mask;

      delta_decoder x_dec{roots_.at(root_idx).x()};
      delta_decoder y_dec{roots_.at(root_idx).y()};

      for (auto i = 0ul; i < span_size; ++i) {
        auto x = x_dec.decode(
            pz::decode_zigzag64(pz::decode_varint(&dat_it, std::end(dat_))));
        auto y = y_dec.decode(
            pz::decode_zigzag64(pz::decode_varint(&dat_it, std::end(dat_))));

        if (curr_id == id) {
          return fixed_xy{x, y};
        }
        ++curr_id;
      }

      curr_id += pz::decode_varint(&dat_it, std::end(dat_));  // empty span
    }

    return std::nullopt;
  }

  osmium::detail::mmap_vector_file<id_offset> idx_;
  osmium::detail::mmap_vector_file<char> dat_;

  std::vector<fixed_xy> roots_;
  uint32_t root_idx_bits_;
};

struct hybrid_node_idx_builder {
  hybrid_node_idx_builder(
      int idx_fd, int dat_fd,
      std::vector<fixed_xy> roots = {{kFixedCoordMagicOffset,
                                      kFixedCoordMagicOffset}})
      : nodes_{idx_fd, dat_fd, std::move(roots)} {
    push_varint(nodes_.roots_.size());
    for (auto const& root : nodes_.roots_) {
      push_varint(root.x());
      push_varint(root.y());
    }
  }

  void push(osm_id_t const id, fixed_xy const& pos) {
    verify(id > last_id_, "ids not sorted!");

    if (last_id_ + 1 != id && !span_.empty()) {
      push_coord_span();
      push_empty_span(id);
    }

    last_id_ = id;
    span_.emplace_back(pos);
  }

  void finish() {
    push_coord_span();

    nodes_.dat_.push_back('\0');  // 0 length empty span
    nodes_.dat_.push_back('\0');  // 0 length full span -> "EOF"
    nodes_.dat_.push_back('\1');  // ensure zeros are actually written?!
  }

  void push_coord_span() {
    if (span_.empty()) {
      return;
    }

    if (nodes_.idx_.empty() || coords_written_ > kCoordsPerIndex) {
      osm_id_t const start_id = last_id_ - span_.size() + 1;
      nodes_.idx_.push_back(id_offset{start_id, nodes_.dat_.size()});
      coords_written_ = 0;
    }

    auto const best_root = get_best_root(span_.front());
    push_varint(span_.size() << nodes_.root_idx_bits_ | best_root);

    delta_encoder x_enc{nodes_.roots_[best_root].x()};
    delta_encoder y_enc{nodes_.roots_[best_root].y()};
    for (auto const& coord : span_) {
      auto const s0 = nodes_.dat_.size();
      push_varint(pz::encode_zigzag64(x_enc.encode(coord.x())));
      auto const s1 = nodes_.dat_.size();
      push_varint(pz::encode_zigzag64(y_enc.encode(coord.y())));
      auto const s2 = nodes_.dat_.size();

      auto const sx = s1 - s0;
      auto const sy = s2 - s1;
      ++stat_coord_chars_[sx < 10 ? sx : 0];
      ++stat_coord_chars_[sy < 10 ? sy : 0];
    }

    stat_nodes_ += span_.size();
    ++stat_spans_;

    for (auto i = 0u; i < kStatSpanCumSizeLimits.size(); ++i) {
      if (span_.size() <= kStatSpanCumSizeLimits[i]) {
        ++stat_span_cum_sizes_[i];
      }
    }

    coords_written_ += span_.size();
    span_.clear();
  }

  size_t get_best_root(fixed_xy const& coord) {
    // XXX faster implementation / precomputed decision tree
    auto dists = utl::to_vec(nodes_.roots_, [&coord](auto const& root) {
      auto x =
          pz::encode_zigzag64(static_cast<fixed_delta_t>(coord.x() - root.x()));
      auto x_bytes = std::max(1., std::ceil(std::floor(std::log2(x) + 1) / 7));

      auto y =
          pz::encode_zigzag64(static_cast<fixed_delta_t>(coord.y() - root.y()));
      auto y_bytes = std::max(1., std::ceil(std::floor(std::log2(y) + 1) / 7));

      return x_bytes + y_bytes;
    });

    return std::distance(begin(dists),
                         std::min_element(begin(dists), end(dists)));
  }

  void push_empty_span(osm_id_t const next_id) {
    push_varint(next_id - last_id_ - 1);
  }

  template <typename Integer64>
  void push_varint(Integer64 v) {
    pz::write_varint(std::back_inserter(nodes_.dat_), v);
  }

  hybrid_node_idx nodes_;

  osm_id_t last_id_ = 0;
  std::vector<fixed_xy> span_;

  static constexpr auto const kCoordsPerIndex = 1024;  // one idx entry each n
  size_t coords_written_ = 0;

  size_t stat_nodes_ = 0;
  size_t stat_spans_ = 0;
  std::array<size_t, 10> stat_coord_chars_ = {};

  static constexpr std::array<size_t, 10> kStatSpanCumSizeLimits{
      1, 7, 8, 9, 15, 16, 17, 100, 1000, 10000};
  std::array<size_t, kStatSpanCumSizeLimits.size()> stat_span_cum_sizes_ = {};
};

}  // namespace tiles

TEST_CASE("hybrid_node_idx") {
  SECTION("empty idx") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(nodes.get_coords(0));
  }

  SECTION("entry single") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.push(42, {2, 3});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(nodes.get_coords(0));
    CHECK_FALSE(nodes.get_coords(100));

    auto result = nodes.get_coords(42);
    REQUIRE(result);
    CHECK(2 == result->x());
    CHECK(3 == result->y());
  }

  SECTION("entries consecutive") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.push(42, {2, 3});
      builder.push(43, {5, 6});
      builder.push(44, {8, 9});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(nodes.get_coords(0));
    CHECK_FALSE(nodes.get_coords(100));

    auto const result42 = nodes.get_coords(42);
    REQUIRE(result42);
    CHECK(2 == result42->x());
    CHECK(3 == result42->y());

    auto const result43 = nodes.get_coords(43);
    REQUIRE(result43);
    CHECK(5 == result43->x());
    CHECK(6 == result43->y());

    auto const result44 = nodes.get_coords(44);
    REQUIRE(result44);
    CHECK(8 == result44->x());
    CHECK(9 == result44->y());
  }

  SECTION("entries gap") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.push(42, {2, 3});
      builder.push(44, {8, 9});
      builder.push(45, {1, 2});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(nodes.get_coords(0));
    CHECK_FALSE(nodes.get_coords(100));

    auto const result42 = nodes.get_coords(42);
    REQUIRE(result42);
    CHECK(2 == result42->x());
    CHECK(3 == result42->y());

    CHECK_FALSE(nodes.get_coords(43));

    auto const result44 = nodes.get_coords(44);
    REQUIRE(result44);
    CHECK(8 == result44->x());
    CHECK(9 == result44->y());

    auto const result45 = nodes.get_coords(45);
    REQUIRE(result45);
    CHECK(1 == result45->x());
    CHECK(2 == result45->y());

    CHECK_FALSE(nodes.get_coords(46));
  }

  SECTION("non-standard root") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd, {{10, 8}}};
      builder.push(42, {2, 3});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(nodes.get_coords(0));
    CHECK_FALSE(nodes.get_coords(100));

    auto result = nodes.get_coords(42);
    REQUIRE(result);
    CHECK(2 == result->x());
    CHECK(3 == result->y());
  }

  SECTION("multiple roots") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{
          idx_fd, dat_fd, {{10, 10}, {50, 70}}};
      builder.push(42, {2, 3});
      builder.push(45, {64, 84});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(nodes.get_coords(0));
    CHECK_FALSE(nodes.get_coords(100));

    auto const result42 = nodes.get_coords(42);
    REQUIRE(result42);
    CHECK(2 == result42->x());
    CHECK(3 == result42->y());

    CHECK_FALSE(nodes.get_coords(43));
    CHECK_FALSE(nodes.get_coords(44));

    auto const result45 = nodes.get_coords(45);
    REQUIRE(result45);
    CHECK(64 == result45->x());
    CHECK(84 == result45->y());

    CHECK_FALSE(nodes.get_coords(46));
  }
}

TEST_CASE("hybrid_node_idx_benchmark", "[!hide]") {
  tiles::t_log("start");

  std::vector<geo::latlng> roots{{-1.93323, 29.0039}, {41.1125, -118.301},
                                 {23.0797, 118.301},  {-21.1255, -57.832},
                                 {48.8069, 8.26172},  {54.2652, 38.4961},
                                 {40.8471, -81.7383}};

  auto const fixed_roots = utl::to_vec(roots, tiles::latlng_to_fixed);

  auto const idx_fd = osmium::detail::create_tmp_file();
  auto const dat_fd = osmium::detail::create_tmp_file();
  tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd, fixed_roots};

  // osmium::io::Reader reader("/data/osm/europe-latest.osm.pbf",
  // osmium::io::Reader reader("/data/osm/hessen-latest.osm.pbf",
  osmium::io::Reader reader("/data/osm/planet-latest.osm.pbf",
                            osmium::osm_entity_bits::node);
  for (auto it = osmium::io::begin(reader); it != osmium::io::end(reader);
       ++it) {
    auto const& node = static_cast<osmium::Node&>(*it);  // NOLINT

    builder.push(node.id(), tiles::latlng_to_fixed({node.location().lat(),
                                                    node.location().lon()}));
  }

  tiles::t_log("index size: {}", builder.nodes_.idx_.size());
  tiles::t_log("data size: {}", builder.nodes_.dat_.size());

  tiles::t_log("builder: nodes {}", builder.stat_nodes_);
  tiles::t_log("builder: spans {}", builder.stat_spans_);

  for (auto i = 0u; i < builder.stat_coord_chars_.size(); ++i) {
    tiles::t_log("builder: coord chars {} {}", i, builder.stat_coord_chars_[i]);
  }

  for (auto i = 0u; i < builder.kStatSpanCumSizeLimits.size(); ++i) {
    tiles::t_log("builder: cum spans <= {:>5} {:>12}",
                 builder.kStatSpanCumSizeLimits[i],
                 builder.stat_span_cum_sizes_[i]);
  }
}

// [29.0039, -1.93323],
// [-118.301, 41.1125],
// [118.301, 23.0797],
// [-57.832, -21.1255],
// [8.26172, 48.8069],
// [38.4961, 54.2652],
// [-81.7383, 40.8471],

namespace tiles {

constexpr auto const kClusterZ = 10;
constexpr auto const kClusterFieldSize = 1ul << kClusterZ;

constexpr auto kClusterCount = 15;
// constexpr auto kClusterCount = 7;
using cluster_centers_t =
    std::array<std::pair<uint32_t, uint32_t>, kClusterCount>;

template <typename Generator>
cluster_centers_t random_centers(Generator& gen,
                                 std::vector<uint32_t> const& weights) {
  std::discrete_distribution<uint32_t> init_dist(begin(weights), end(weights));
  cluster_centers_t centers;

  auto const make_random_center = [&] {
    std::pair<uint32_t, uint32_t> c_new{};
    while (c_new == std::pair<uint32_t, uint32_t>{} ||
           std::find(begin(centers), end(centers), c_new) != end(centers)) {
      auto const c = init_dist(gen);
      c_new.first = c % kClusterFieldSize;
      c_new.second = c / kClusterFieldSize;
    }
    return c_new;
  };

  for (auto i = 0; i < kClusterCount; ++i) {
    centers[i] = make_random_center();
  }
  return centers;
}

struct cluster_solution {
  size_t iter_;
  size_t error_;
  cluster_centers_t centers_;
};

cluster_solution k_medians(cluster_centers_t const& init,
                           std::vector<uint32_t> const& weights) {
  auto const diff = [](auto const& a, auto const& b) {
    return (a > b) ? (a - b) : (b - a);
  };

  std::vector<cluster_solution> solutions;
  cluster_centers_t curr = init;
  for (auto iter = 0ull; iter < 100ull; ++iter) {
    std::array<std::array<size_t, kClusterFieldSize>, kClusterCount> x_dists{
        {}};
    std::array<std::array<size_t, kClusterFieldSize>, kClusterCount> y_dists{
        {}};
    for (auto y = 0u; y < kClusterFieldSize; ++y) {
      for (auto x = 0u; x < kClusterFieldSize; ++x) {
        auto it = std::min_element(
            begin(curr), end(curr), [&](auto const& a, auto const& b) {
              return diff(a.first, x) + diff(a.second, y) <
                     diff(b.first, x) + diff(b.second, y);
            });
        verify(it != end(curr), "no best cluster");

        do {
          auto const c = std::distance(begin(curr), it);
          auto const w = weights[y * kClusterFieldSize + x];

          x_dists[c][x] += w;
          y_dists[c][y] += w;

        } while (
            (it = std::find_if(std::next(it), end(curr), [&](auto const& a) {
               return diff(it->first, x) + diff(it->second, y) ==
                      diff(a.first, x) + diff(a.second, y);
             })) != end(curr));
      }
    }

    for (auto i = 0u; i < kClusterCount; ++i) {
      auto const sum2 =
          std::accumulate(begin(x_dists[i]), end(x_dists[i]), 0ull) / 2;
      size_t x_acc = 0;
      for (auto j = 0u; j < kClusterFieldSize; ++j) {
        x_acc += x_dists[i][j];
        if (x_acc >= sum2) {
          curr[i].first = j;
          break;
        }
      }

      size_t y_acc = 0;
      for (auto j = 0u; j < kClusterFieldSize; ++j) {
        y_acc += y_dists[i][j];
        if (y_acc >= sum2) {
          curr[i].second = j;
          break;
        }
      }
    }

    size_t error = 0;
    for (auto y = 0u; y < kClusterFieldSize; ++y) {
      for (auto x = 0u; x < kClusterFieldSize; ++x) {
        auto const it = std::min_element(
            begin(curr), end(curr), [&](auto const& a, auto const& b) {
              return diff(a.first, x) + diff(a.second, y) <
                     diff(b.first, x) + diff(b.second, y);
            });
        verify(it != end(curr), "no best cluster");
        error += weights[y * kClusterFieldSize + x] *
                 (diff(it->first, x) + diff(it->second, y));
      }
    }
    // tiles::t_log("iter {:>3} : {}", iter, error);
    solutions.push_back({iter, error, curr});

    constexpr auto const kNoImprovement = 5;
    if (solutions.size() >= kNoImprovement) {
      auto const first = std::next(end(solutions), -kNoImprovement);
      auto const it = std::min_element(
          first, end(solutions),
          [&](auto const& a, auto const& b) { return a.error_ < b.error_; });

      if (it == first) {
        return *it;
      }
    }
  }

  return solutions.back();
}

}  // namespace tiles

using namespace tiles;

TEST_CASE("cluster_seed_kmeans", "[!hide]") {
  size_t node_count = 0;
  std::vector<uint32_t> weights(kClusterFieldSize * kClusterFieldSize, 0u);

  // osmium::io::Reader reader("/data/osm/hessen-latest.osm.pbf",
  // osmium::io::Reader reader("/data/osm/germany-latest.osm.pbf",
  // osmium::io::Reader reader("/data/osm/europe-latest.osm.pbf",
  osmium::io::Reader reader("/data/osm/planet-latest.osm.pbf",
                            osmium::osm_entity_bits::node);
  for (auto it = osmium::io::begin(reader); it != osmium::io::end(reader);
       ++it) {
    auto const& node = static_cast<osmium::Node&>(*it);  // NOLINT

    auto const pos =
        geo::latlng_to_merc({node.location().lat(), node.location().lon()});
    uint32_t const x =
        std::clamp(
            static_cast<long>(tiles::proj::merc_to_pixel_x(pos.x_, kClusterZ)),
            0l, static_cast<long>(proj::map_size(kClusterZ) - 1)) /
        tiles::proj::kTileSize;
    uint32_t const y =
        std::clamp(
            static_cast<long>(tiles::proj::merc_to_pixel_y(pos.y_, kClusterZ)),
            0l, static_cast<long>(proj::map_size(kClusterZ) - 1)) /
        tiles::proj::kTileSize;

    if (x >= kClusterFieldSize || y >= kClusterFieldSize) {
      t_log("{} {} | {} {}", x, y,
            tiles::proj::merc_to_pixel_x(pos.x_, kClusterZ),
            tiles::proj::merc_to_pixel_y(pos.y_, kClusterZ));
    }

    verify(x < kClusterFieldSize && y < kClusterFieldSize, "invalid tile");

    ++weights[y * kClusterFieldSize + x];
    ++node_count;
  }

  std::cout << " w" << std::accumulate(begin(weights), end(weights), 0ull)
            << std::endl;

  tiles::t_log("node count: {}", node_count);

  cluster_centers_t centers{};

  std::mt19937 gen(42);
  std::vector<cluster_solution> solutions;
  for (auto i = 0; i < 100; ++i) {
    solutions.emplace_back(k_medians(random_centers(gen, weights), weights));
  }

  auto const it = std::min_element(
      begin(solutions), end(solutions),
      [&](auto const& a, auto const& b) { return a.error_ < b.error_; });
  verify(it != end(solutions), "no solution");

  tiles::t_log("best ({}/{}) with error {} after ({} iter)",
               std::distance(begin(solutions), it) + 1, solutions.size(),
               it->error_, it->iter_);
  for (auto const& center : it->centers_) {
    auto const merc_x = tiles::proj::pixel_to_merc_x(
        tiles::proj::kTileSize * center.first + tiles::proj::kTileSize / 2,
        kClusterZ);
    auto const merc_y = tiles::proj::pixel_to_merc_y(
        tiles::proj::kTileSize * center.second + tiles::proj::kTileSize / 2,
        kClusterZ);

    auto const latlng = geo::merc_to_latlng({merc_x, merc_y});
    std::cout << "[" << latlng.lng_ << ", " << latlng.lat_ << "],\n";
  }

  // for (auto const& center : centers) {
  //   std::cout << center.first << " " << center.second << std::endl;
  // }

  // uint32_t best_it = 0;

  // for (auto i = 0u; i < kClusterCount; ++i) {
  //   // auto const sum =
  //       // std::accumulate(begin(x_dists[i]), end(x_dists[i]), 0ull);
  //   std::cout << centers[i].first << " " << centers[i].second << " : " << sum
  //             << std::endl;
  // }
  // assignments[y * kZ + x] = std::distance(begin(centers), it);
}
