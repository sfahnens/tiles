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
    init_root_idx();
  }

  hybrid_node_idx(int idx_fd, int dat_fd, std::vector<fixed_xy> roots)
      : idx_{idx_fd}, dat_{dat_fd}, roots_{std::move(roots)} {
    init_root_idx();
    verify(!roots_.empty(), "roots must not be empty");
  }

  void init_root_idx() {
    root_idx_bits_ = std::floor(std::log2(roots_.size()) + 1);
    root_idx_mask_ = (static_cast<size_t>(1) << root_idx_bits_) - 1;

    // last element must must be available as tag for empty span
    verify(roots_.size() <= root_idx_mask_, "root computation mask broken!");
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
      auto const span_size = header >> root_idx_bits_;
      auto const root_idx = header & root_idx_mask_;

      if (root_idx == root_idx_mask_) {
        if (span_size == 0) {
          break;  // eof
        } else {
          curr_id += span_size;  // empty span
          continue;
        }
      }

      delta_decoder x_dec{roots_.at(root_idx).x()};
      delta_decoder y_dec{roots_.at(root_idx).y()};

      for (auto i = 0ul; i < (span_size + 1); ++i) {
        auto x = x_dec.decode(
            pz::decode_zigzag64(pz::decode_varint(&dat_it, std::end(dat_))));
        auto y = y_dec.decode(
            pz::decode_zigzag64(pz::decode_varint(&dat_it, std::end(dat_))));

        if (curr_id == id) {
          return fixed_xy{x, y};
        }
        ++curr_id;
      }
    }

    return std::nullopt;
  }

  osmium::detail::mmap_vector_file<id_offset> idx_;
  osmium::detail::mmap_vector_file<char> dat_;

  std::vector<fixed_xy> roots_;

  size_t root_idx_bits_;
  size_t root_idx_mask_;
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

    t_log("reading {} roots ({} bits)", nodes_.roots_.size(),
          nodes_.root_idx_bits_);
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
    push_empty_span(last_id_ + 1);  // 0 length empty span --> "EOF"

    // nodes_.dat_.push_back('\0');  // 0 length empty span
    // nodes_.dat_.push_back('\0');  // 0 length full span -> "EOF"
    // nodes_.dat_.push_back('\1');  // ensure zeros are actually written?!
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

    struct coord_info {
      int best_root_, restart_saving_;
    };

    // 1. forward pass - compute best roots and savings per node
    auto const best_init_root = get_best_root(span_.front());
    delta_encoder x_enc{nodes_.roots_[best_init_root].x()};
    delta_encoder y_enc{nodes_.roots_[best_init_root].y()};
    auto const info = utl::to_vec(span_, [&](auto const& coord) {
      auto const& best_root_idx = get_best_root(coord);

      auto const restart_cost =
          byte_distance(nodes_.roots_[best_root_idx], coord);
      auto const consec_cost =
          get_varint_size(pz::encode_zigzag64(x_enc.encode(coord.x()))) +
          get_varint_size(pz::encode_zigzag64(y_enc.encode(coord.y())));

      return coord_info{best_root_idx, consec_cost - restart_cost};
    });

    // 2. backward pass - find actual restarts / span sizes
    std::vector<size_t> span_sizes;
    size_t curr_size = 0;
    for (int i = span_.size() - 1; i >= 0; --i) {
      ++curr_size;
      if (get_varint_size((curr_size - 1) << nodes_.root_idx_bits_) <
          info[i].restart_saving_) {
        span_sizes.push_back(curr_size);
        curr_size = 0;
      }
    }
    if (curr_size != 0) {
      span_sizes.push_back(curr_size);
    }
    std::reverse(begin(span_sizes), end(span_sizes));

    // 3. write pass - write using computed sizes
    auto coord_idx = 0;
    for (auto const& span_size : span_sizes) {
      auto const& best_root = info[coord_idx].best_root_;
      push_varint((span_size - 1) << nodes_.root_idx_bits_ | best_root);

      x_enc.reset(nodes_.roots_[best_root].x());
      y_enc.reset(nodes_.roots_[best_root].y());
      for (auto i = 0u; i < span_size; ++i, ++coord_idx) {
        auto x0 = x_enc.curr_;
        auto y0 = y_enc.curr_;

        auto const sx = push_varint_zz(x_enc.encode(span_[coord_idx].x()));
        auto const sy = push_varint_zz(y_enc.encode(span_[coord_idx].y()));

        if (debug_limit_ < 100 && (sx == 5 || sy == 5)) {
          auto pos0 = tiles::fixed_to_latlng({x0, y0});
          auto pos = tiles::fixed_to_latlng(span_[coord_idx]);
          std::cout << "[[" << pos0.lng_ << ", " << pos0.lat_ << "],";
          std::cout << "[" << pos.lng_ << ", " << pos.lat_ << "]],\t";
          std::cout << i << " " << sx << " " << sy << "\n";

          for (auto const& root : nodes_.roots_) {
            auto const& from = root;
            auto const& to = span_[coord_idx];


          auto pos_r = tiles::fixed_to_latlng(root);

          std::cout << pos_r.lng_ << "," << pos_r.lat_ << "\t ";

          std::cout << from.x() << "," << from.y() << "\t ";
          std::cout << to.x() << "," << to.y() << "\t ";

            auto x = pz::encode_zigzag64(
                static_cast<fixed_delta_t>(to.x() - from.x()));
            auto y = pz::encode_zigzag64(
                static_cast<fixed_delta_t>(to.y() - from.y()));

            std::cout << static_cast<fixed_delta_t>(to.x() - from.x()) << "|"
                      << static_cast<fixed_delta_t>(to.y() - from.y()) << " ";

            std::cout << x << "|" << y << " ";
            std::cout << get_varint_size(x) << "+" << get_varint_size(y)
                      << "\n";
          }

          std::cout << "\n";

          ++debug_limit_;
        }

        ++stat_coord_chars_[sx < 10 ? sx : 0];
        ++stat_coord_chars_[sy < 10 ? sy : 0];
      }

      stat_nodes_ += span_size;
      ++stat_spans_;

      for (auto i = 0u; i < kStatSpanCumSizeLimits.size(); ++i) {
        if (span_size <= kStatSpanCumSizeLimits[i]) {
          ++stat_span_cum_sizes_[i];
        }
      }
    }

    coords_written_ += span_.size();
    span_.clear();
  }

  int byte_distance(fixed_xy const& from, fixed_xy const& to) {
    auto x = pz::encode_zigzag64(static_cast<fixed_delta_t>(to.x() - from.x()));
    auto y = pz::encode_zigzag64(static_cast<fixed_delta_t>(to.y() - from.y()));
    return get_varint_size(x) + get_varint_size(y);
  }

  int get_best_root(fixed_xy const& coord) {
    auto const dists = utl::to_vec(nodes_.roots_, [&](auto const& root) {
      return byte_distance(root, coord);
    });

    return std::distance(begin(dists),
                         std::min_element(begin(dists), end(dists)));
  }

  static inline int get_varint_size(uint64_t value) {
    int n = 1;
    while (value >= 0x80u) {
      value >>= 7u;
      ++n;
    }
    return n;
  }

  void push_empty_span(osm_id_t const next_id) {
    push_varint((next_id - last_id_ - 1) << nodes_.root_idx_bits_ |
                nodes_.root_idx_mask_);
  }

  template <typename Integer64>
  int push_varint(Integer64 v) {
    return pz::write_varint(std::back_inserter(nodes_.dat_), v);
  }

  template <typename Integer64>
  int push_varint_zz(Integer64 v) {
    return push_varint(pz::encode_zigzag64(v));
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

  size_t debug_limit_ = 0;
};

}  // namespace tiles

#define CHECK_EXISTS(nodes, id, pos_x, pos_y) \
  {                                           \
    auto const result = nodes.get_coords(id); \
    REQUIRE(result);                          \
    CHECK(pos_x == result->x());              \
    CHECK(pos_y == result->y());              \
  }

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

    CHECK_EXISTS(nodes, 42, 2, 3);
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

    CHECK_EXISTS(nodes, 42, 2, 3);
    CHECK_EXISTS(nodes, 43, 5, 6);
    CHECK_EXISTS(nodes, 44, 8, 9);
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

    CHECK_FALSE(nodes.get_coords(41));
    CHECK_FALSE(nodes.get_coords(43));
    CHECK_FALSE(nodes.get_coords(46));

    CHECK_EXISTS(nodes, 42, 2, 3);
    CHECK_EXISTS(nodes, 44, 8, 9);
    CHECK_EXISTS(nodes, 45, 1, 2);
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

    CHECK_EXISTS(nodes, 42, 2, 3);
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

    CHECK_EXISTS(nodes, 42, 2, 3);
    CHECK_EXISTS(nodes, 45, 64, 84);

    CHECK_FALSE(nodes.get_coords(41));
    CHECK_FALSE(nodes.get_coords(43));
    CHECK_FALSE(nodes.get_coords(44));
    CHECK_FALSE(nodes.get_coords(46));
  }

  SECTION("artificial splits") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{
          idx_fd, dat_fd, {{123456, 0}, {0, 123456}, {10, 10}}};
      builder.push(42, {2, 3});
      builder.push(43, {2, 7});
      builder.push(44, {123450, 2});
      builder.push(45, {123455, 10});
      builder.push(46, {8, 123450});
      builder.push(47, {3, 123455});
      builder.finish();

      CHECK(3 == builder.stat_spans_);
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(nodes.get_coords(0));
    CHECK_FALSE(nodes.get_coords(100));

    CHECK_FALSE(nodes.get_coords(41));
    CHECK_FALSE(nodes.get_coords(48));

    CHECK_EXISTS(nodes, 42, 2, 3);
    CHECK_EXISTS(nodes, 43, 2, 7);
    CHECK_EXISTS(nodes, 44, 123450, 2);
    CHECK_EXISTS(nodes, 45, 123455, 10);
    CHECK_EXISTS(nodes, 46, 8, 123450);
    CHECK_EXISTS(nodes, 47, 3, 123455);
  }
}

TEST_CASE("hybrid_node_idx_benchmark", "[!hide]") {
  tiles::t_log("start");

  // std::vector<geo::latlng> roots{{-1.93323, 29.0039}, {41.1125, -118.301},
  //                                {23.0797, 118.301},  {-21.1255, -57.832},
  //                                {48.8069, 8.26172},  {54.2652, 38.4961},
  //                                {40.8471, -81.7383}};

  std::vector<geo::latlng> roots{
      {22.7559, 87.7148},  {51.9443, 6.15234}, {-22.106, -56.7773},
      {44.9648, 1.58203},  {-7.1881, 120.762}, {35.6037, 135.527},
      {47.3983, 38.1445},  {9.27562, 5.09766}, {55.4789, 69.082},
      {-11.6953, 31.8164}, {60.6732, 23.3789}, {45.4601, -119.004},
      {42.6824, -75.7617}, {48.1074, 15.293},  {35.3174, -92.9883}};

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
