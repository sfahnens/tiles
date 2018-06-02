#include "tiles/osm/load_coastlines.h"

#include <stack>

#include "blockingconcurrentqueue.h"

#include "tiles/db/insert_feature.h"
#include "tiles/db/tile_database.h"
#include "tiles/osm/load_shapefile.h"

#include "tiles/fixed/algo/area.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/algo/clip.h"
#include "tiles/fixed/convert.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/mvt/tile_spec.h"

namespace sc = std::chrono;

namespace tiles {

using geo_queue_t =
    moodycamel::BlockingConcurrentQueue<std::pair<uint32_t, fixed_geometry>>;
using db_queue_t =
    moodycamel::BlockingConcurrentQueue<std::pair<geo::tile, std::string>>;

void process_coastline(uint32_t clip_limit, fixed_geometry geo,  //
                       geo_queue_t& geo_queue, db_queue_t& db_queue) {
  auto const box = bounding_box(geo);

  uint32_t const idx_z = 10;
  auto const idx_range = make_tile_range(box, idx_z);

  uint32_t clip_z = idx_z;
  auto clip_range = idx_range;
  while (clip_z > clip_limit && ++clip_range.begin() != clip_range.end()) {
    clip_range = geo::tile_range_on_z(clip_range, --clip_z);
  }

  if (clip_z < idx_z) {
    for (auto const& tile : clip_range) {
      tile_spec spec{tile};
      auto clipped = clip(geo, spec.draw_bounds_);

      if (std::holds_alternative<fixed_null>(clipped)) {
        continue;
      }

      if (std::holds_alternative<fixed_polygon>(clipped)) {
        auto const& polygon = std::get<fixed_polygon>(clipped);
        if (polygon.size() == 1 && polygon.front().outer().size() == 5 &&
            polygon.front().inners().empty() &&
            area(spec.draw_bounds_) == area(polygon)) {
          std::cout << "found fully filled: " << tile << std::endl;
          continue;
        }
      }

      geo_queue.enqueue({clip_limit + 1, std::move(clipped)});
    }
  } else {
    for (auto const& tile : idx_range) {
      tile_spec spec{tile};
      auto clipped = clip(geo, spec.draw_bounds_);

      if (std::holds_alternative<fixed_null>(clipped)) {
        continue;
      }

      feature f{0ul,
                std::pair<uint32_t, uint32_t>{0, kMaxZoomLevel + 1},
                {{"layer", "coastline"}},
                std::move(clipped)};
      db_queue.enqueue({tile, serialize_feature(f)});
    }
  }
}

void load_coastlines(tile_db_handle& handle, std::string const& fname) {
  geo_queue_t geo_queue;
  db_queue_t db_queue;

  std::atomic_bool init{true};
  auto const num_workers = std::thread::hardware_concurrency();

  std::vector<std::thread> threads;
  std::vector<std::atomic_bool> work(num_workers);
  for (auto i = 0u; i < num_workers; ++i) {
    threads.emplace_back([&, i] {
      while (true) {
        std::pair<uint32_t, fixed_geometry> geo;
        auto has_task =
            geo_queue.wait_dequeue_timed(geo, sc::milliseconds(100));

        work[i] = has_task;
        if (!init && std::none_of(begin(work), end(work),
                                  [](auto& w) { return w.load(); })) {
          break;
        }
        if (!has_task) {
          continue;
        }

        process_coastline(geo.first, std::move(geo.second),  //
                          geo_queue, db_queue);
      }
    });
  }

  {
    auto coastlines = load_shapefile(fname);  // TODO consumer lambda
    std::cout << "loaded " << coastlines.size() << " coastlines" << std::endl;
    for (auto& coastline : coastlines) {
      geo_queue.enqueue({4, std::move(coastline)});
    }
    init = false;
  }
  {
    // FIXME map must be shared with other features inserter
    feature_inserter inserter{handle, &tile_db_handle::features_dbi};
    while (true) {
      std::pair<geo::tile, std::string> data;
      auto has_task = db_queue.wait_dequeue_timed(data, sc::milliseconds(100));

      if (!has_task) {
        if (!init && std::none_of(begin(work), end(work),
                                  [](auto& w) { return w.load(); })) {
          break;
        }
        continue;
      }

      auto const idx = inserter.fill_state_[{data.first.x_, data.first.y_}]++;
      auto const key = make_feature_key(data.first, idx);

      inserter.insert(key, data.second);
    }
  }

  std::for_each(begin(threads), end(threads), [](auto& t) { t.join(); });
}

}  // namespace tiles
