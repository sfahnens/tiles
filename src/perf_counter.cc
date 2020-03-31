#include "tiles/perf_counter.h"

#include <algorithm>
#include <iostream>

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "tiles/util.h"

namespace tiles {

template <typename Printable>
void print(char const* label, std::vector<uint64_t>& m) {
  auto const sum = std::accumulate(begin(m), end(m), 0.);
  std::sort(begin(m), end(m));

  fmt::print(std::cout, "{:<18} > cnt: {} sum: {}", label,
             printable_num{m.size()}, Printable{sum});

  if (m.empty()) {
    std::cout << "\n";
    return;
  }

  fmt::print(std::cout, " mean: {} q95: {} max: {}\n",
             Printable{sum / m.size()}, Printable{m[m.size() * .95]},
             Printable{m.back()});
}

void perf_report_get_tile(perf_counter& pc) {
  print<printable_bytes>(" RESULT: SIZE", pc.finished_[perf_task::RESULT_SIZE]);

  print<printable_ns>(" GET: TOTAL", pc.finished_[perf_task::GET_TILE_TOTAL]);
  print<printable_ns>(" GET: FETCH", pc.finished_[perf_task::GET_TILE_FETCH]);
  print<printable_ns>(" GET: RENDER", pc.finished_[perf_task::GET_TILE_RENDER]);
  print<printable_ns>(" GET: COMPRESS",
                      pc.finished_[perf_task::GET_TILE_COMPRESS]);

  print<printable_ns>("RNDR: FIND SEASIDE",
                      pc.finished_[perf_task::RENDER_TILE_FIND_SEASIDE]);
  print<printable_ns>("RNDR: ADD SEASIDE",
                      pc.finished_[perf_task::RENDER_TILE_ADD_SEASIDE]);

  print<printable_ns>("RNDR: QUERY FEAT",
                      pc.finished_[perf_task::RENDER_TILE_QUERY_FEATURE]);
  print<printable_ns>("RNDR: ITER FEAT",
                      pc.finished_[perf_task::RENDER_TILE_ITER_FEATURE]);
  print<printable_ns>("RNDR: DESER OKAY",
                      pc.finished_[perf_task::RENDER_TILE_DESER_FEATURE_OKAY]);
  print<printable_ns>("RNDR: DESER SKIP",
                      pc.finished_[perf_task::RENDER_TILE_DESER_FEATURE_SKIP]);
  print<printable_ns>("RNDR: ADD FEAT",
                      pc.finished_[perf_task::RENDER_TILE_ADD_FEATURE]);
  print<printable_ns>("RNDR: FINISH",
                      pc.finished_[perf_task::RENDER_TILE_FINISH]);
}

}  // namespace tiles
