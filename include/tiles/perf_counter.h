#pragma once

#include <chrono>
#include <array>
#include <iostream>
#include <numeric>
#include <vector>

namespace tiles {

namespace perf_task {
enum perf_task_t : uint32_t {
  RESULT_SIZE,

  GET_TILE_TOTAL,
  GET_TILE_FETCH,
  GET_TILE_RENDER,
  GET_TILE_COMPRESS,

  RENDER_TILE_FIND_SEASIDE,
  RENDER_TILE_ADD_SEASIDE,

  RENDER_TILE_QUERY_FEATURE,
  RENDER_TILE_ITER_FEATURE,
  RENDER_TILE_DESER_FEATURE_OKAY,
  RENDER_TILE_DESER_FEATURE_SKIP,
  RENDER_TILE_ADD_FEATURE,
  RENDER_TILE_FINISH,

  SIZE
};
}  // namespace perf_task

struct perf_counter {
  using clock_t = std::chrono::high_resolution_clock;
  using time_point_t = std::chrono::time_point<clock_t>;

  static constexpr auto const kInvalidTimePoint = time_point_t::max();

  perf_counter() {
    for (auto i = 0ULL; i < running_.size(); ++i) {
      running_[i] = kInvalidTimePoint;
    }
  }

  template <perf_task::perf_task_t Task>
  void append(uint64_t const value) {
    finished_[Task].push_back(value);
  }

  template <perf_task::perf_task_t Task>
  void start() {
    running_[Task] = clock_t::now();
  }

  template <perf_task::perf_task_t Task>
  void stop() {
    auto const end = clock_t::now();
    auto const& start = running_[Task];

    if (start == kInvalidTimePoint) {
      return;
    }

    using namespace std::chrono;
    finished_[Task].push_back(duration_cast<nanoseconds>(end - start).count());
    running_[Task] = kInvalidTimePoint;
  }

  std::array<time_point_t, perf_task::SIZE> running_;
  std::array<std::vector<uint64_t>, perf_task::SIZE> finished_;
};

struct null_perf_counter {
  template <perf_task::perf_task_t Task>
  void append(uint64_t) {}

  template <perf_task::perf_task_t Task>
  void start() {}

  template <perf_task::perf_task_t Task>
  void stop() {}
};

template <perf_task::perf_task_t Task, typename PerfCounter>
void start(PerfCounter& pc) {
  pc.template start<Task>();
}

template <perf_task::perf_task_t Task, typename PerfCounter>
void stop(PerfCounter& pc) {
  pc.template stop<Task>();
}

template <perf_task::perf_task_t Task, typename PerfCounter>
struct scoped_perf_counter_impl {
  explicit scoped_perf_counter_impl(PerfCounter& pc) : pc_{&pc} {
    pc_->template start<Task>();
  }
  ~scoped_perf_counter_impl() {
    if (pc_ != nullptr) {
      pc_->template stop<Task>();
    }
  }

  scoped_perf_counter_impl(scoped_perf_counter_impl&& other) noexcept {
    std::swap(pc_, other.pc_);
  }

  scoped_perf_counter_impl& operator=(
      scoped_perf_counter_impl&& other) noexcept {
    std::swap(pc_, other.pc_);
    return *this;
  }

  scoped_perf_counter_impl(scoped_perf_counter_impl const&) = delete;
  scoped_perf_counter_impl& operator=(scoped_perf_counter_impl const&) = delete;

  PerfCounter* pc_ = nullptr;
};

template <perf_task::perf_task_t Task, typename PerfCounter>
scoped_perf_counter_impl<Task, PerfCounter> scoped_perf_counter(
    PerfCounter& pc) {
  return scoped_perf_counter_impl<Task, PerfCounter>{pc};
}

void perf_report_get_tile(perf_counter&);

}  // namespace tiles
