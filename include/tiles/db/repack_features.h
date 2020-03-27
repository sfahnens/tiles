#pragma once

#include <numeric>
#include <stack>
#include <vector>

#include "geo/tile.h"

#include "utl/concat.h"
#include "utl/pairwise.h"
#include "utl/to_vec.h"

#include "tiles/db/pack_file.h"
#include "tiles/util.h"
#include "tiles/util_parallel.h"

namespace tiles {

template <typename T, typename MergePred, typename MergeOp, typename RemovePred>
void merge_remove_erase(std::vector<T>& vec, MergePred&& merge_pred,
                        MergeOp&& merge_op, RemovePred&& remove_pred) {
  auto it = begin(vec);
  for (; it != end(vec) && std::next(it) != end(vec); ++it) {
    if (merge_pred(*it, *std::next(it)) || remove_pred(*it)) {
      break;
    }
  }

  if (it != end(vec) && std::next(it) != end(vec)) {
    for (auto it2 = std::next(it); it2 != end(vec); ++it2) {
      if (merge_pred(*it, *it2)) {
        merge_op(*it, *it2);  // merge consecutive
      } else if (remove_pred(*it)) {
        *it = std::move(*it2);  // overwrite elements
      } else {
        *(++it) = std::move(*it2);  // just move
      }
    }
    vec.erase(std::next(it), end(vec));
  }

  if (remove_pred(*it)) {
    vec.pop_back();
  }
}

// -- -- -- -- -- -- -- --

struct tile_record {
  geo::tile tile_;
  std::vector<pack_record> records_;
};

struct tile_record_single {
  tile_record_single() = default;
  tile_record_single(geo::tile tile, pack_record record)
      : tile_{tile}, record_{record} {}

  geo::tile tile_;
  pack_record record_;
};

// -- -- -- -- -- -- -- --

struct owned_pack_record {
  owned_pack_record() = default;
  owned_pack_record(tile_record* task, pack_record record)
      : task_{task}, record_{record} {}

  friend bool operator<(owned_pack_record const& a,
                        owned_pack_record const& b) {
    return a.record_.offset_ < b.record_.offset_;
  }

  tile_record* task_;
  pack_record record_;
};

struct record_block_node {
  static constexpr auto const kInvalidOffset =
      std::numeric_limits<size_t>::max();

  record_block_node* parent_{nullptr};
  std::unique_ptr<record_block_node> left_, right_;
  size_t min_offset_{0};

  size_t max_gap_{0};
  bool dirty_{false};

  size_t record_count_{0};
  std::unique_ptr<std::vector<owned_pack_record>> payload_;
};

inline std::unique_ptr<record_block_node> make_record_block_tree(
    std::vector<std::unique_ptr<record_block_node>> nodes) {
  while (nodes.size() > 1) {
    if (nodes.size() % 2 == 1) {
      nodes.emplace_back(std::make_unique<record_block_node>());
      nodes.back()->min_offset_ = record_block_node::kInvalidOffset;
    }
    for (auto i = 0UL; i < nodes.size(); i += 2) {
      auto n = std::make_unique<record_block_node>();
      n->left_ = std::move(nodes.at(i));
      n->right_ = std::move(nodes.at(i + 1));

      n->left_->parent_ = n.get();
      n->right_->parent_ = n.get();

      n->record_count_ = n->left_->record_count_ + n->right_->record_count_;
      n->max_gap_ = std::max(n->left_->max_gap_, n->right_->max_gap_);
      n->min_offset_ = n->left_->min_offset_;

      utl::verify(n->left_->min_offset_ < n->right_->min_offset_,
                  "make_record_block_tree: not sorted by min_offset {} < {}",
                  n->left_->min_offset_, n->right_->min_offset_);

      nodes[i / 2] = std::move(n);
    }
    nodes.resize(nodes.size() / 2);
  }

  return std::move(nodes.at(0));
}

template <typename Fn>
void foreach_leaf(std::unique_ptr<record_block_node>& init, Fn&& fn) {
  using ref_t = std::reference_wrapper<std::unique_ptr<record_block_node>>;
  std::stack<ref_t> s{{init}};
  while (!s.empty()) {
    auto& n = s.top().get();
    s.pop();
    if (n->right_) {
      s.push(n->right_);
    }
    if (n->left_) {  // stack -> will be popped first!
      s.push(n->left_);
    }
    if (n->payload_) {
      fn(n);
    }
  }
}

// -- -- -- -- -- -- -- --

constexpr size_t const kRepackInFlightMemory = 128ul * 1024 * 1024;
constexpr auto kRepackBatchSize = 32;
constexpr auto kTargetRecordBlockSize = 2048;

template <typename PackHandle>
struct repack_memory_manager {
  repack_memory_manager(PackHandle& pack_handle,
                        std::vector<tiles::tile_record> tasks)
      : pack_handle_{pack_handle},
        tasks_{std::move(tasks)},
        tasks_left_{tasks_.size()} {
    init_record_block_tree();
  }

  void init_record_block_tree() {
    std::vector<owned_pack_record> tmp;
    for (auto& task : tasks_) {
      for (auto const& record : task.records_) {
        tmp.emplace_back(&task, record);
      }
    }

    std::sort(begin(tmp), end(tmp), [](auto const& a, auto const& b) {
      return a.record_.offset_ < b.record_.offset_;
    });
    for (auto i = begin(tmp); i != end(tmp) && std::next(i) != end(tmp); ++i) {
      utl::verify(i->record_.offset_ + i->record_.size_ ==
                      std::next(i)->record_.offset_,
                  "init_record_block_tree: have gaps (not implemented)");
    }

    // prepare blocks
    std::vector<std::unique_ptr<record_block_node>> nodes;
    for (auto i = 0UL; i < tmp.size(); i += kTargetRecordBlockSize) {
      auto n = std::make_unique<record_block_node>();
      n->payload_ = std::make_unique<std::vector<owned_pack_record>>(
          std::next(begin(tmp), i),
          std::next(begin(tmp),
                    std::min(i + kTargetRecordBlockSize, tmp.size())));

      utl::verify(!n->payload_->empty(), "init_record_block_tree: no payload");
      n->record_count_ = n->payload_->size();
      n->min_offset_ = n->payload_->front().record_.offset_;
      nodes.emplace_back(std::move(n));
    }

    root_ = make_record_block_tree(std::move(nodes));
  }

  bool have_more_tasks() const { return tasks_left_ != 0; }

  tile_record dequeue_task() {
    utl::verify(have_more_tasks(), "dequeue_task: no more tasks");

    auto const extract = [&](auto const* task) {
      for (auto const& record : task->records_) {
        auto* n = root_.get();
        while (!n->payload_) {
          utl::verify(n->left_ && n->left_, "dequeue_task: invalid tree");
          n = record.offset_ < n->right_->min_offset_ ? n->left_.get()
                                                      : n->right_.get();
        }

        auto it =
            std::find_if(begin(*n->payload_), end(*n->payload_),
                         [&](auto const& r) { return r.record_ == record; });
        utl::verify(it != end(*n->payload_),
                    "dequeue_task: missing pack_record");
        it->task_ = nullptr;
        freed_space_since_housekeeping_ += it->record_.size_;
        n->dirty_ = true;
      }
      --tasks_left_;
      return *task;
    };

    while (true) {
      record_block_node* n = root_.get();
      while (!n->payload_) {
        n = n->left_.get();
      }
      for (auto& [task, record] : *n->payload_) {
        if (task) {
          return extract(task);
        }
      }
      std::cout << "B" << std::endl;
      tree_housekeeping_front();
    }

    utl::verify(false, "TODO should not happen");
    return {};
  }

  owned_pack_record& first_record() {
    record_block_node* n = root_.get();
    while (!n->payload_) {
      n = n->left_.get();
    }

    utl::verify(!n->payload_->empty(), "insert_result: imbalanced tree");
    return n->payload_->front();
  }

  void insert_result(geo::tile const tile, std::string const& buf) {
    if (!have_more_tasks()) {  // no more pending tasks!
      updates_.emplace_back(tile, pack_handle_.append(buf));
      return;
    }

    auto const try_insert = [&] {
      record_block_node* n = root_.get();
      while (!n->payload_) {
        n = n->left_.get();
      }

      utl::verify(!n->payload_->empty(), "insert_result: imbalanced tree");
      auto& [owner, record] = n->payload_->front();

      // canidate is owned a tasks or there is not enough space
      if (owner != nullptr || record.size_ < buf.size()) {
        return false;
      }

      updates_.emplace_back(tile, pack_handle_.insert(record.offset_, buf));
      record.offset_ += updates_.back().record_.size_;
      record.size_ -= updates_.back().record_.size_;

      n->dirty_ = true;
      return true;
    };

    if (try_insert()) {
      return;
    }

    make_insertion_space();

    if (try_insert()) {
      return;
    }

    back_stash_.emplace_back(tile, pack_handle_.append(buf));
  }

  template <typename Callback>
  void housekeeping_post_enqueue(Callback const& callback) {
    if (!have_more_tasks()) {
      finish_back_stash();
      callback(updates_);
      updates_.clear();
    }
  }

  template <typename Callback>
  void housekeeping_flush(Callback const& callback) {
    callback(updates_);
    updates_.clear();
  }

  void tree_housekeeping_front() {
    utl::verify(root_ != nullptr, "root is nullptr");
    record_block_node* n = root_.get();
    while (!n->payload_) {
      n = n->left_.get();
    }

    utl::verify(n != root_.get(), "front is root");
    utl::verify(!n->payload_->empty(), "payload is empty");

    size_t offset = n->payload_->front().record_.offset_;
    size_t size = 0;
    for (auto& [task, record] : *n->payload_) {
      utl::verify(task == nullptr, "non empty in front");
      size += record.size_;
    }

    if (n->parent_ == root_.get()) {
      root_ = std::move(n->parent_->right_);
      root_->parent_ = nullptr;
    } else {
      auto* gp = n->parent_->parent_;
      gp->left_ = std::move(n->parent_->right_);
      gp->left_->parent_ = gp;
    }

    n = root_.get();
    while (!n->payload_) {
      n = n->left_.get();
    }

    n->payload_->emplace_back(nullptr, pack_record{offset, size});
    update_gap_vec(n->record_count_, *n->payload_);
    n->min_offset_ = offset;
    n->max_gap_ = 0;  // dont insert front*;
    n->dirty_ = false;
    propagate_metadata(n);
  }

  void tree_housekeeping(bool force = false) {
    if (!force && freed_space_since_housekeeping_ < kRepackInFlightMemory / 2) {
      return;
    }
    freed_space_since_housekeeping_ = 0;

    scoped_timer t{"tree_housekeeping"};
    utl::verify(root_ != nullptr, "tree_housekeeping: root missing");

    bool have_small_nodes = false;
    foreach_leaf(root_, [&](auto& n) {
      if (n->dirty_) {
        update_gap_vec(n->record_count_, *n->payload_);
        defragment_leaf(n.get());

        if (n->payload_->size() > kTargetRecordBlockSize * 2) {
          split_node(*n);
        } else if (n.get() != root_.get() &&
                   n->payload_->size() < kTargetRecordBlockSize / 2) {
          have_small_nodes = true;
        }
        n->dirty_ = false;
      }
    });

    if (have_small_nodes) {
      std::vector<std::unique_ptr<record_block_node>> nodes;
      foreach_leaf(root_, [&](auto& n) {
        if (n->min_offset_ != record_block_node::kInvalidOffset) {
          nodes.emplace_back(std::move(n));
        }
      });

      for (auto const& [a, b] : utl::pairwise(nodes)) {
        utl::verify(a->min_offset_ < b->min_offset_,
                    "tree_housekeeping: not sorted (1)");
      }

      merge_remove_erase(
          nodes,
          [](auto const& a, auto const& b) {
            return a->payload_->size() < kTargetRecordBlockSize / 2 ||
                   b->payload_->size() < kTargetRecordBlockSize / 2;
          },
          [](auto& a, auto const& b) {
            utl::concat(*a->payload_, *b->payload_);
            a->record_count_ = a->payload_->size();
            a->max_gap_ = std::max(a->max_gap_, b->max_gap_);
          },
          [](auto const& e) { return e->payload_->size() == 0; });

      for (auto const& [a, b] : utl::pairwise(nodes)) {
        utl::verify(a->min_offset_ < b->min_offset_,
                    "tree_housekeeping: not sorted (1)");
      }

      root_ = make_record_block_tree(std::move(nodes));
    }
  }

  void update_gap_vec(size_t& count, std::vector<owned_pack_record>& vec) {
    if (vec.empty()) {
      count = 0;
      return;
    }

    auto const mid_it = begin(vec) + count;
    std::sort(mid_it, end(vec));
    std::inplace_merge(begin(vec), mid_it, end(vec));

    merge_remove_erase(
        vec,
        [](auto const& a, auto const& b) {
          return a.task_ == nullptr && b.task_ == nullptr &&
                 a.record_.offset_ + a.record_.size_ == b.record_.offset_;
        },
        [](auto& a, auto const& b) { a.record_.size_ += b.record_.size_; },
        [](auto const& e) {
          return e.task_ == nullptr && e.record_.size_ == 0;
        });

    count = vec.size();
  }

  void defragment_leaf(record_block_node* n) {
    utl::verify(n->payload_ && !n->payload_->empty(),
                "try to defrag node without payload");
    size_t insert_offset = n->payload_->front().record_.offset_;
    size_t end_offset =
        n->payload_->back().record_.offset_ + n->payload_->back().record_.size_;

    for (auto& [task, record] : *n->payload_) {
      if (record.offset_ == insert_offset || task == nullptr) {
        continue;
      }

      auto it = std::find(begin(task->records_), end(task->records_), record);
      utl::verify(it != end(task->records_), "defragment_leaf: record missing");
      record = pack_handle_.move(insert_offset, record);
      *it = record;
      insert_offset += record.size_;
    }

    if (insert_offset != end_offset) {
      utl::erase_if(*n->payload_,
                    [](auto const& r) { return r.task_ == nullptr; });
      n->payload_->emplace_back(
          nullptr, pack_record(insert_offset, end_offset - insert_offset));
      n->record_count_ = n->payload_->size();
      n->max_gap_ = n->payload_->back().record_.size_;

      propagate_metadata(n);
    }
  }

  void propagate_metadata(record_block_node* n) {
    while (n->parent_ != nullptr) {
      n = n->parent_;
      n->record_count_ = n->left_->record_count_ + n->right_->record_count_;
      n->max_gap_ = std::max(n->left_->max_gap_, n->right_->max_gap_);
    }
  }

  void split_node(record_block_node&) {}

  void make_insertion_space() {
    tree_housekeeping();

    auto const move = [&](auto* n0, auto& task, auto& record) {
      record_block_node* n = root_.get();
      while (!n->payload_) {
        n = (n->right_->min_offset_ == record_block_node::kInvalidOffset ||
             n->right_->max_gap_ < record.size_)
                ? n->left_.get()
                : n->right_.get();

        if (n->max_gap_ < record.size_) {
          return false;
        }
      }
      utl::verify(n != n0, "insert in source");

      for (size_t g = n->record_count_; g != 0; --g) {
        auto& gap = n->payload_->at(g - 1);
        if (gap.record_.offset_ <= record.offset_) {
          return false;
        }
        if (gap.task_ != nullptr || gap.record_.size_ < record.size_) {
          continue;
        }

        auto it = std::find(begin(task->records_), end(task->records_), record);
        utl::verify(it != end(task->records_),
                    "make_insertion_space: record missing");

        // move element
        *it = pack_handle_.move(gap.record_.offset_, record);

        // update/shrink gap
        gap.record_.offset_ += record.size_;
        gap.record_.size_ -= record.size_;

        // register record
        n->payload_->push_back({task, *it});

        // mark old space as free
        task = nullptr;

        n->max_gap_ = 0;
        for (auto const& [task, record] : *n->payload_) {
          if (task == nullptr) {
            n->max_gap_ = std::max(n->max_gap_, record.size_);
          }
        }
        propagate_metadata(n);

        n->dirty_ = true;
        return true;
      }

      utl::verify(false, "could not find a matching gap -> broken state?");
      return false;
    };

    for (auto i = 0; i < 4; ++i) {
      record_block_node* n = root_.get();
      while (!n->payload_) {
        n = n->left_.get();
      }
      for (auto& [task, record] : *n->payload_) {
        if (task && !move(n, task, record)) {
          goto finish_moving;
        }
      }
      std::cout << "A" << std::endl;
      tree_housekeeping_front();
    }
  finish_moving:
    tree_housekeeping();

    // auto const extract = [&](auto const* task) {
    //   for (auto const& record : task->records_) {
    //     auto* n = root_.get();
    //     while (!n->payload_) {
    //       utl::verify(n->left_ && n->left_, "dequeue_task: invalid tree");
    //       n = record.offset_ < n->right_->min_offset_ ? n->left_.get()
    //                                                   : n->right_.get();
    //     }

    //     auto it =
    //         std::find_if(begin(*n->payload_), end(*n->payload_),
    //                      [&](auto const& r) { return r.record_ == record; });
    //     utl::verify(it != end(*n->payload_),
    //                 "dequeue_task: missing pack_record");
    //     it->task_ = nullptr;
    //     freed_space_since_housekeeping_ += it->record_.size_;
    //     n->dirty_ = true;
    //   }
    //   --tasks_left_;
    //   return *task;
    // };

    // record_block_node* n = root_.get();
    // while (!n->payload_) {
    //   n = n->left_.get();
    // }
    // for (auto& [task, record] : *n->payload_) {
    //   if (task) {
    //     return extract(task);
    //   }
    // }

    // tree_housekeeping(true);

    // root_.get();
    // while (!n->payload_) {
    //   n = n->left_.get();
    // }
    // for (auto& [task, record] : *n->payload_) {
    //   if (task) {
    //     return extract(task);
    //   }
    // }

    tree_housekeeping();
  }

  auto const finish_back_stash() {
    utl::verify(!have_more_tasks(), "finish_back_stash: task queue not empty");

    record_block_node* n = root_.get();
    while (!n->payload_) {
      n = n->left_.get();
    }

    utl::verify(!n->payload_->empty(), "insert_result: imbalanced tree");
    auto& [task, record] = n->payload_->front();
    utl::verify(task == nullptr, "finish_back_stash: there is still a task");

    size_t insert_offset = record.offset_;
    for (auto const& [tile, from_record] : back_stash_) {
      auto to_record = pack_handle_.move(insert_offset, from_record);
      insert_offset += to_record.size_;
      updates_.emplace_back(tile, to_record);
    }

    pack_handle_.resize(insert_offset);
    back_stash_.clear();
    root_ = nullptr;
  }

  PackHandle& pack_handle_;
  std::vector<tile_record> tasks_;
  size_t tasks_left_;

  std::unique_ptr<record_block_node> root_;
  std::vector<tile_record_single> back_stash_;

  size_t freed_space_since_housekeeping_{0};

  std::vector<tile_record_single> updates_;
};

template <typename PackHandle, typename PackFeatures, typename Callback>
void repack_features(PackHandle& pack_handle, std::vector<tile_record> in_tasks,
                     PackFeatures&& pack_features, Callback&& callback) {
  utl::erase_if(in_tasks, [](auto const& t) { return t.records_.empty(); });
  for (auto& task : in_tasks) {
    std::sort(begin(task.records_), end(task.records_));
  }
  repack_memory_manager<PackHandle> mgr{pack_handle, std::move(in_tasks)};

  progress_tracker pack_progress{"pack features", mgr.tasks_.size()};

  queue_wrapper<std::function<void()>> work_queue;
  queue_wrapper<std::pair<geo::tile, std::string>> result_queue;

  auto const enqueue_work = [&](auto n) {
    auto const enqueue = [&] {
      auto task = mgr.dequeue_task();
      auto packs = utl::to_vec(task.records_, [&](auto const& r) {
        return std::string{pack_handle.get(r)};
      });
      work_queue.enqueue([&, tile = task.tile_, packs = std::move(packs)] {
        result_queue.enqueue({tile, pack_features(tile, packs)});
      });

      return std::accumulate(
          begin(task.records_), end(task.records_), 0ul,
          [](auto acc, auto const& r) { return acc + r.size_; });
    };

    if (n == -1) {
      size_t enqueued_size = 0;
      while (mgr.have_more_tasks() && enqueued_size < kRepackInFlightMemory) {
        enqueued_size += enqueue();
      }

      std::cout << "enqueued " << enqueued_size << std::endl;
    } else {
      while (mgr.have_more_tasks() && n > 0) {
        enqueue();
        --n;
      }
    }
  };

  auto dequeue_results = [&](auto n) {
    auto const dequeue = [&] {
      std::pair<geo::tile, std::string> result;
      if (result_queue.dequeue(result)) {
        mgr.insert_result(result.first, result.second);
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
  };

  {
    queue_processor proc{work_queue};
    enqueue_work(-1);  // some more as buffer
    while (mgr.have_more_tasks()) {
      dequeue_results(kRepackBatchSize);
      mgr.housekeeping_flush(callback);
      enqueue_work(kRepackBatchSize);
    }
    mgr.finish_back_stash();
    mgr.housekeeping_flush(callback);

    dequeue_results(-1);  // ensure fully drained
    mgr.housekeeping_flush(callback);
  }

  t_log("pack file utilization: {:.2f}% ({} / {})",
        100. * pack_handle.size() / pack_handle.capacity(),  //
        pack_handle.size(), pack_handle.capacity());

  utl::verify(!mgr.have_more_tasks(), "pack_features: task queue not empty");
  utl::verify(work_queue.queue_.size_approx() == 0,
              "pack_features: work queue not empty");
  utl::verify(result_queue.queue_.size_approx() == 0,
              "pack_features: result queue not empty");
}

}  // namespace tiles
