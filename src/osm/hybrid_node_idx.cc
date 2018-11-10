#include "tiles/osm/hybrid_node_idx.h"

#include <limits>
#include <tuple>

#include "osmium/index/detail/mmap_vector_file.hpp"
#include "osmium/index/detail/tmpfile.hpp"

#include "protozero/varint.hpp"

#include "tiles/fixed/algo/delta.h"
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

struct hybrid_node_idx::impl {
  impl(int idx_fd, int dat_fd) : idx_{idx_fd}, dat_{dat_fd} {}

  osmium::detail::mmap_vector_file<id_offset> idx_;
  osmium::detail::mmap_vector_file<char> dat_;
};

uint32_t read_fixed(char const** data) {
  auto p = reinterpret_cast<const int8_t*>(*data);
  uint32_t val = 0;
  val |= *p++;
  val |= (*p++) << 8;
  val |= (*p++) << 16;
  val |= (*p++) << 24;
  *data = reinterpret_cast<const char*>(p);
  return val;
}

std::optional<fixed_xy> get_coords(hybrid_node_idx const& nodes,
                                   osm_id_t const& id) {
  auto const& idx = nodes.impl_->idx_;
  auto const& dat = nodes.impl_->dat_;

  if (idx.empty()) {  // XXX very unlikely in prod
    return std::nullopt;
  }
  auto it =
      std::lower_bound(std::begin(idx), std::end(idx), id,
                       [](auto const& o, auto const& i) { return o.id_ < i; });

  if (it == std::begin(idx) && it->id_ != id) {  // not empty -> begin != end
    return std::nullopt;
  }
  if (it == std::end(idx) || it->id_ != id) {
    --it;
  }

  osm_id_t curr_id = it->id_;
  auto dat_it = dat.data() + it->offset_;

  while (curr_id <= id && dat_it != std::end(dat)) {
    auto const header = pz::decode_varint(&dat_it, std::end(dat));
    auto const span_size = header >> 1;
    auto const root_idx = header & 0x1;

    if (root_idx == 0x1) {
      if (span_size == 0) {
        break;  // eof
      } else {
        curr_id += span_size;  // empty span
        continue;
      }
    }

    auto const x0 = read_fixed(&dat_it);
    auto const y0 = read_fixed(&dat_it);
    if (curr_id == id) {
      return fixed_xy{x0, y0};
    }
    ++curr_id;

    delta_decoder x_dec{x0};
    delta_decoder y_dec{y0};
    for (auto i = 1ul; i < (span_size + 1); ++i) {
      auto x = x_dec.decode(
          pz::decode_zigzag64(pz::decode_varint(&dat_it, std::end(dat))));
      auto y = y_dec.decode(
          pz::decode_zigzag64(pz::decode_varint(&dat_it, std::end(dat))));

      if (curr_id == id) {
        return fixed_xy{x, y};
      }
      ++curr_id;
    }
  }

  return std::nullopt;
}

hybrid_node_idx::hybrid_node_idx()
    : impl_{std::make_unique<impl>(osmium::detail::create_tmp_file(),
                                   osmium::detail::create_tmp_file())} {}
hybrid_node_idx::hybrid_node_idx(int idx_fd, int dat_fd)
    : impl_{std::make_unique<impl>(idx_fd, dat_fd)} {}
hybrid_node_idx::~hybrid_node_idx() = default;

struct hybrid_node_idx_builder::impl {
  impl(osmium::detail::mmap_vector_file<id_offset>& idx,
       osmium::detail::mmap_vector_file<char>& dat)
      : nodes_{nullptr}, idx_{idx}, dat_{dat} {}

  impl(std::unique_ptr<hybrid_node_idx::impl> nodes)
      : nodes_{std::move(nodes)}, idx_{nodes_->idx_}, dat_{nodes_->dat_} {}

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
  }

  void push_coord_span() {
    if (span_.empty()) {
      return;
    }

    if (idx_.empty() || coords_written_ > kCoordsPerIndex) {
      osm_id_t const start_id = last_id_ - span_.size() + 1;
      idx_.push_back(id_offset{start_id, dat_.size()});
      coords_written_ = 0;
    }

    delta_encoder x_enc{0};
    delta_encoder y_enc{0};
    for (auto i = 0u; i < span_.size();) {
      x_enc.reset(span_[i].x());
      y_enc.reset(span_[i].y());

      auto j = i + 1;
      for (; j < span_.size(); ++j) {
        auto const sx =
            get_varint_size(pz::encode_zigzag64(x_enc.encode(span_[j].x())));
        auto const sy =
            get_varint_size(pz::encode_zigzag64(y_enc.encode(span_[j].y())));

        if (sx + sy > 2 * sizeof(uint32_t) + 1) {
          break;
        }

        ++stat_coord_chars_[sx < 10 ? sx : 0];
        ++stat_coord_chars_[sy < 10 ? sy : 0];
      }

      auto const span_size = j - i;
      push_varint((span_size - 1) << 1 | 0x0);
      push_fixed(span_[i].x());
      push_fixed(span_[i].y());

      x_enc.reset(span_[i].x());
      y_enc.reset(span_[i].y());

      ++i;
      for (; i < j; ++i) {
        push_varint_zz(x_enc.encode(span_[i].x()));
        push_varint_zz(y_enc.encode(span_[i].y()));
      }

      ++stat_spans_;
      for (auto k = 0u; k < kStatSpanCumSizeLimits.size(); ++k) {
        if (span_size <= kStatSpanCumSizeLimits[k]) {
          ++stat_span_cum_sizes_[k];
        }
      }
    }

    coords_written_ += span_.size();
    span_.clear();
  }

  void push_empty_span(osm_id_t const next_id) {
    push_varint((next_id - last_id_ - 1) << 1 | 0x1);
  }

  template <typename Integer64>
  int push_varint(Integer64 v) {
    return pz::write_varint(std::back_inserter(dat_), v);
  }

  template <typename Integer64>
  int push_varint_zz(Integer64 v) {
    return push_varint(pz::encode_zigzag64(v));
  }

  void push_fixed(uint32_t v) {
    auto data = std::back_inserter(dat_);
    for (auto i = 0u; i < sizeof(v); ++i) {
      *data++ = char(v & 0xffu);
      v >>= 8u;
    }
  }

  static inline uint32_t get_varint_size(uint64_t value) {
    uint32_t n = 1;
    while (value >= 0x80u) {
      value >>= 7u;
      ++n;
    }
    return n;
  }

  void dump_stats() const {
    tiles::t_log("index size: {} entries", idx_.size());
    tiles::t_log("data size: {} bytes", dat_.size());

    tiles::t_log("bulder: nodes {}", stat_nodes_);
    tiles::t_log("builder: spans {}", stat_spans_);

    for (auto i = 0u; i < stat_coord_chars_.size(); ++i) {
      tiles::t_log("builder: coord chars {} {}", i, stat_coord_chars_[i]);
    }

    for (auto i = 0u; i < kStatSpanCumSizeLimits.size(); ++i) {
      tiles::t_log("builder: cum spans <= {:>5} {:>12}",
                   kStatSpanCumSizeLimits[i], stat_span_cum_sizes_[i]);
    }
  }

  std::unique_ptr<hybrid_node_idx::impl> nodes_;
  osmium::detail::mmap_vector_file<id_offset>& idx_;
  osmium::detail::mmap_vector_file<char>& dat_;

  osm_id_t last_id_ = 0;
  std::vector<fixed_xy> span_;

  static constexpr auto const kCoordsPerIndex = 1024;  // one idx entry each n
  size_t coords_written_ = 0;

  size_t stat_nodes_ = 0;
  size_t stat_spans_ = 0;
  std::array<size_t, 10> stat_coord_chars_ = {};

  static constexpr std::array<size_t, 10> kStatSpanCumSizeLimits{1, 64, 100,
                                                                 1000, 10000};
  std::array<size_t, kStatSpanCumSizeLimits.size()> stat_span_cum_sizes_ = {};
};

hybrid_node_idx_builder::hybrid_node_idx_builder(hybrid_node_idx& nodes)
    : impl_{std::make_unique<impl>(nodes.impl_->idx_, nodes.impl_->dat_)} {}

hybrid_node_idx_builder::hybrid_node_idx_builder(int idx_fd, int dat_fd)
    : impl_{std::make_unique<impl>(
          std::make_unique<hybrid_node_idx::impl>(idx_fd, dat_fd))} {}

hybrid_node_idx_builder::~hybrid_node_idx_builder() = default;

void hybrid_node_idx_builder::push(osmium::object_id_type const id,
                                   fixed_xy const& coords) {
  impl_->push(id, coords);
}

void hybrid_node_idx_builder::finish() { impl_->finish(); }

void hybrid_node_idx_builder::dump_stats() const { impl_->dump_stats(); }
size_t hybrid_node_idx_builder::get_stat_spans() const {
  return impl_->stat_spans_;
}

}  // namespace tiles