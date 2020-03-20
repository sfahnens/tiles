#include "tiles/db/feature_pack.h"

#include <numeric>
#include <optional>

#include "utl/concat.h"
#include "utl/equal_ranges.h"
#include "utl/equal_ranges_linear.h"
#include "utl/erase_if.h"
#include "utl/to_vec.h"
#include "utl/verify.h"

#include "tiles/db/pack_file.h"
#include "tiles/db/quad_tree.h"
#include "tiles/db/shared_metadata.h"
#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/feature/deserialize.h"
#include "tiles/feature/feature.h"
#include "tiles/feature/serialize.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/io/dump.h"
#include "tiles/mvt/tile_spec.h"
#include "tiles/util_parallel.h"

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
      utl::verify(it->size() >= 32, "MINI FEATURE?!");
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
  while (best.z_ < kMaxZoomLevel) {
    std::optional<geo::tile> next_best;
    for (auto const& child : best.direct_children()) {
      auto const tile_box = tile_spec{child}.insert_bounds_;
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

    utl::verify(next_best.has_value(), "at least one child must match");
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
    utl::verify(trace.back().z_ > root.z_, "tile outside root");
    trace.push_back(trace.back().parent());
  }
  trace.push_back(root);
  std::reverse(begin(trace), end(trace));

  return utl::to_vec(trace,
                     [](auto const& t) -> uint8_t { return t.quad_pos(); });
}

struct packable_feature {
  packable_feature(std::vector<uint8_t> quad_key, geo::tile best_tile,
                   std::string feature)
      : quad_key_{std::move(quad_key)},
        best_tile_{std::move(best_tile)},
        feature_{std::move(feature)} {}

  friend bool operator<(packable_feature const& a, packable_feature const& b) {
    return std::tie(a.quad_key_, a.best_tile_, a.feature_) <
           std::tie(b.quad_key_, b.best_tile_, b.feature_);
  }

  char const* data() const { return feature_.data(); }
  size_t size() const { return feature_.size(); }

  std::vector<uint8_t> quad_key_;
  geo::tile best_tile_;
  std::string feature_;
};

std::string pack_features(geo::tile const& tile,
                          shared_metadata_coder const& metadata_coder,
                          std::vector<std::string_view> const& strings) {

  std::vector<std::vector<packable_feature>> features_by_min_z(kMaxZoomLevel +
                                                               1 - tile.z_);
  for (auto const& str : strings) {
    auto const feature = deserialize_feature(str, metadata_coder);
    utl::verify(feature.has_value(), "feature must be valid (!?)");

    auto const str2 = serialize_feature(*feature, metadata_coder, false);

    auto const best_tile = find_best_tile(tile, *feature);
    auto const z = std::max(tile.z_, feature->zoom_levels_.first) - tile.z_;
    features_by_min_z.at(z).emplace_back(make_quad_key(tile, best_tile),
                                         best_tile, std::move(str2));
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

struct tile_record {
  geo::tile tile_;
  std::vector<pack_record> records_;
};

void pack_features(tile_db_handle& db_handle, pack_handle& pack_handle) {
  auto const metadata_coder = make_shared_metadata_coder(db_handle);

  std::vector<tile_record> tasks;
  {
    auto txn = db_handle.make_txn();
    auto feature_dbi = db_handle.features_dbi(txn);
    lmdb::cursor c{txn, feature_dbi};

    for (auto el = c.get<tile_index_t>(lmdb::cursor_op::FIRST); el;
         el = c.get<tile_index_t>(lmdb::cursor_op::NEXT)) {
      auto tile = feature_key_to_tile(el->first);
      auto records = pack_records_deserialize(el->second);
      utl::verify(!records.empty(), "pack_features: empty pack_records");

      if (!tasks.empty() && tasks.back().tile_ == tile) {
        utl::concat(tasks.back().records_, records);
      } else {
        tasks.push_back({tile, records});
      }
    }

    txn.dbi_clear(feature_dbi);
    txn.commit();
  }

  for (auto& task : tasks) {
    std::sort(begin(task.records_), end(task.records_));
  }
  std::sort(begin(tasks), end(tasks), [](auto const& lhs, auto const& rhs) {
    return lhs.records_.front().offset_ > rhs.records_.front().offset_;
  });

  progress_tracker pack_progress{"pack features", tasks.size()};

  queue_wrapper<std::function<void()>> work_queue;
  queue_wrapper<std::pair<geo::tile, std::string>> result_queue;

  auto const enqueue_work = [&](auto n) {
    auto const enqueue = [&] {
      auto task = std::move(tasks.back());
      tasks.pop_back();

      auto parts = utl::to_vec(task.records_, [&](auto const& r) {
        return std::string{pack_handle.get(r)};
      });

      work_queue.enqueue([&, tile = task.tile_, parts = std::move(parts)] {
        std::vector<std::string_view> features;
        for (auto const& part : parts) {
          unpack_features(part, [&features](auto const& view) {
            features.emplace_back(view);
          });
        }
        result_queue.enqueue(
            {tile, pack_features(tile, metadata_coder, features)});
      });

      return std::accumulate(
          begin(task.records_), end(task.records_), 0ul,
          [](auto acc, auto const& r) { return acc + r.size_; });
    };

    if (n == -1) {
      size_t enqueued_size = 0;
      while (!tasks.empty() && enqueued_size < 128ul * 1024 * 1024) {
        enqueued_size += enqueue();
      }
    } else {
      while (!tasks.empty() && n > 0) {
        enqueue();
        --n;
      }
    }
  };

  size_t insert_offset = 0;
  std::vector<std::pair<geo::tile, pack_record>> back_queue;

  auto const free_space = [&] {
    auto const first_occupied =
        !tasks.empty()
            ? tasks.back().records_.front().offset_
            : (!back_queue.empty() ? back_queue.front().second.offset_
                                   : std::numeric_limits<size_t>::max());
    utl::verify(insert_offset <= first_occupied,
                "insert_compact: invalid state");

    return first_occupied - insert_offset;
  };

  size_t insert_compact_size = 0;
  size_t insert_back_queue_size = 0;

  auto const insert_compact = [&](auto const& tile, auto const& buf) {
    if (free_space() >= buf.size()) {
      auto record = pack_handle.insert(insert_offset, buf);
      insert_offset += buf.size();
      insert_compact_size += buf.size();
      return std::optional{record};
    } else {
      insert_back_queue_size += buf.size();
      back_queue.emplace_back(tile, pack_handle.append(buf));
      return std::optional<pack_record>{};
    }
  };

  auto dequeue_results = [&](auto n) {
    auto txn = db_handle.make_txn();
    auto feature_dbi = db_handle.features_dbi(txn);

    auto const dequeue = [&] {
      std::pair<geo::tile, std::string> result;
      if (result_queue.dequeue(result)) {
        auto opt_record = insert_compact(result.first, result.second);
        if (opt_record) {
          txn.put(feature_dbi, make_feature_key(result.first),
                  pack_records_serialize(*opt_record));
        }
        result_queue.finish();
        pack_progress.inc();
      }
    };

    if (n == -1) {  // while there is any work left
      while (!(result_queue.finished() && work_queue.finished())) {
        dequeue();
      }
    } else {  // dequeue some (and enqueue some)
      while (n > 0 && !(result_queue.finished() && work_queue.finished())) {
        dequeue();
        --n;
      }
    }

    // back queue management
    while (!back_queue.empty() &&
           back_queue.back().second.size_ <= free_space()) {
      std::cout << "take from back queue" << std::endl;
      auto [tile, from_record] = back_queue.back();
      back_queue.pop_back();

      auto to_record = pack_handle.move(insert_offset, from_record);
      insert_offset += to_record.size_;
      pack_handle.resize(from_record.offset_);

      txn.put(feature_dbi, make_feature_key(tile),
              pack_records_serialize(to_record));
    }

    txn.commit();
  };

  constexpr auto kBatchSize = 32;

  {
    queue_processor proc{work_queue};
    enqueue_work(-1);  // some more as buffer
    while (!tasks.empty()) {
      dequeue_results(kBatchSize);
      enqueue_work(kBatchSize);
    }
    dequeue_results(-1);  // ensure fully drained
  }

  {
    auto txn = db_handle.make_txn();
    auto feature_dbi = db_handle.features_dbi(txn);

    for (auto const& [tile, from_record] : back_queue) {
      auto to_record = pack_handle.move(insert_offset, from_record);
      insert_offset += to_record.size_;
      txn.put(feature_dbi, make_feature_key(tile),
              pack_records_serialize(to_record));
    }

    txn.commit();
    pack_handle.resize(insert_offset);
  }

  t_log("pack file utilization: {:.2f}%",
        100. * pack_handle.size() / pack_handle.capacity());

  utl::verify(tasks.empty(), "pack_features: task queue not empty");
  utl::verify(work_queue.queue_.size_approx() == 0,
              "pack_features: work queue not empty");
  utl::verify(result_queue.queue_.size_approx() == 0,
              "pack_features: result queue not empty");
}

}  // namespace tiles
