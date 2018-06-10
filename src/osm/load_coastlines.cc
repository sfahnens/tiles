#include "tiles/osm/load_coastlines.h"

#include <fstream>
#include <memory>
#include <type_traits>
#include <variant>

#include "blockingconcurrentqueue.h"
#include "clipper/clipper.hpp"

#include "utl/to_vec.h"

#include "tiles/db/insert_feature.h"
#include "tiles/db/tile_database.h"
#include "tiles/osm/load_shapefile.h"

#include "tiles/fixed/algo/area.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/algo/clip.h"
#include "tiles/fixed/convert.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/mvt/tile_spec.h"

namespace cl = ClipperLib;
namespace sc = std::chrono;

namespace tiles {

static_assert(std::is_same_v<cl::cInt, fixed_coord_t>, "coord type problem");

struct coastline {
  coastline(fixed_box box, cl::Paths geo)
      : box_{std::move(box)}, geo_{std::move(geo)} {}

  fixed_box box_;
  cl::Paths geo_;
};
using coastline_ptr = std::shared_ptr<coastline>;

struct geo_task {
  geo::tile tile_;
  std::vector<coastline_ptr> coastlines_;
};

template <typename T>
struct queue_wrapper {
  queue_wrapper() : pending_{0} {}

  void enqueue(T&& t) {
    ++pending_;
    queue_.enqueue(std::forward<T>(t));
  }

  bool dequeue(T& t) {
    return queue_.wait_dequeue_timed(t, sc::milliseconds(100));
  }

  void finish() { --pending_; }
  bool finished() const { return pending_ == 0; }

  std::atomic_uint64_t pending_;
  moodycamel::BlockingConcurrentQueue<T> queue_;
};

using geo_queue_t = queue_wrapper<geo_task>;
using db_queue_t = queue_wrapper<std::pair<geo::tile, std::string>>;

struct coastline_stats {
  coastline_stats() : progress_{0} {}

  void report_progess(uint32_t z) {
    auto increment = 1 << (10 - z) * 1 << (10 - z);

    auto post = progress_ += increment;
    auto pre = post - increment;

    constexpr uint64_t kTotal = (1 << 10) * (1 << 10);

    auto pre_percent = 100. * pre / kTotal;
    auto post_percent = 100. * post / kTotal;

    if (pre == 0 || post == kTotal || (static_cast<int>(pre_percent) / 5 !=
                                       static_cast<int>(post_percent) / 5)) {
      std::cout << "process coastline: "
                << (static_cast<int>(post_percent) / 5 * 5) << "%\n";
    }
  }

  std::atomic_uint64_t progress_;
};

std::ostream& operator<<(std::ostream& os, fixed_box const& box) {
  return os << "(" << box.min_corner().x() << ", " << box.min_corner().y()
            << ")(" << box.max_corner().x() << ", " << box.max_corner().y()
            << ")";
}

fixed_box bounding_box(cl::Paths const& geo) {
  auto min_x = std::numeric_limits<fixed_coord_t>::max();
  auto min_y = std::numeric_limits<fixed_coord_t>::max();
  auto max_x = std::numeric_limits<fixed_coord_t>::min();
  auto max_y = std::numeric_limits<fixed_coord_t>::min();

  for (auto const& path : geo) {
    for (auto const& pt : path) {
      min_x = std::min(min_x, pt.X);
      min_y = std::min(min_y, pt.Y);
      max_x = std::max(max_x, pt.X);
      max_y = std::max(max_y, pt.Y);
    }
  }

  return fixed_box{{min_x, min_y}, {max_x, max_y}};
}

bool touches(fixed_box const& a, fixed_box const& b) {
  return !(a.min_corner().x() > b.max_corner().x() ||
           a.max_corner().x() < b.min_corner().x()) &&
         !(a.min_corner().y() > b.max_corner().y() ||
           a.max_corner().y() < b.min_corner().y());
}

bool within(fixed_box const& outer, fixed_box const& inner) {
  return outer.min_corner().x() <= inner.min_corner().x() &&
         outer.max_corner().x() >= inner.max_corner().x() &&
         outer.min_corner().y() <= inner.min_corner().y() &&
         outer.max_corner().y() >= inner.max_corner().y();
}

cl::Path box_to_path(fixed_box const& box) {
  return {{box.min_corner().x(), box.min_corner().y()},
          {box.max_corner().x(), box.min_corner().y()},
          {box.max_corner().x(), box.max_corner().y()},
          {box.min_corner().x(), box.max_corner().y()}};
}

cl::Paths intersection(cl::Paths const& subject, cl::Path const& clip) {
  cl::Clipper clpr;
  verify(clpr.AddPaths(subject, cl::ptSubject, true), "AddPath failed");
  verify(clpr.AddPath(clip, cl::ptClip, true), "AddPaths failed");

  cl::Paths solution;
  verify(clpr.Execute(cl::ctIntersection, solution, cl::pftEvenOdd,
                      cl::pftEvenOdd),
         "Execute failed");
  return solution;
}

void to_fixed_polygon(fixed_polygon& polygon, cl::PolyNodes const& nodes) {
  auto const path_to_ring = [](auto const& path) {
    verify(!path.empty(), "path empty");
    fixed_ring ring;
    ring.reserve(path.size() + 1);
    for (auto const& pt : path) {
      ring.emplace_back(pt.X, pt.Y);
    }
    ring.emplace_back(path[0].X, path[0].Y);
    return ring;
  };

  for (auto const* outer : nodes) {
    verify(!outer->IsHole(), "outer ring is hole");
    fixed_simple_polygon simple;
    simple.outer() = path_to_ring(outer->Contour);

    for (auto const* inner : outer->Childs) {
      verify(inner->IsHole(), "inner ring is no hole");
      simple.inners().emplace_back(path_to_ring(inner->Contour));

      to_fixed_polygon(polygon, inner->Childs);
    }

    polygon.emplace_back(std::move(simple));
  }
}

std::string finalize_tile(cl::Path const& bounds,
                          std::vector<coastline_ptr> const& coastlines) {
  cl::Clipper clpr;
  clpr.AddPath(bounds, cl::ptSubject, true);
  for (auto const& coastline : coastlines) {
    clpr.AddPaths(coastline->geo_, cl::ptClip, true);
  }

  cl::PolyTree solution;
  clpr.Execute(cl::ctDifference, solution, cl::pftEvenOdd, cl::pftEvenOdd);
  verify(!solution.Childs.empty(), "difference empty!");

  fixed_polygon polygon;
  to_fixed_polygon(polygon, solution.Childs);

  boost::geometry::correct(polygon);
  return serialize_feature({0ul,
                            std::pair<uint32_t, uint32_t>{0, kMaxZoomLevel + 1},
                            {{"layer", "coastline"}},
                            std::move(polygon)});
}

void process_coastline(geo_task& task, geo_queue_t& geo_q, db_queue_t& db_q,
                       coastline_stats& stats) {
  for (auto const& child : task.tile_.direct_children()) {
    // scoped_timer t{"clip"};

    auto const bounds = tile_spec{child}.draw_bounds_;
    auto const clip = box_to_path(bounds);

    bool fully_dirtside = false;
    std::vector<coastline_ptr> matching;
    for (auto const& coastline : task.coastlines_) {
      if (!touches(bounds, coastline->box_)) {
        continue;
      }

      if (within(bounds, coastline->box_)) {
        matching.push_back(coastline);
        continue;
      }

      auto geo = intersection(coastline->geo_, clip);
      if (geo.empty()) {
        continue;
      }

      if (geo.size() == 1 && geo[0].size() == 4 &&
          std::all_of(begin(geo[0]), end(geo[0]), [&clip](auto const& pt) {
            return end(clip) != std::find(begin(clip), end(clip), pt);
          })) {
        fully_dirtside = true;
        break;
      }

      matching.push_back(std::make_shared<struct coastline>(bounding_box(geo),
                                                            std::move(geo)));
    }

    if (fully_dirtside) {
      // std::cout << "found fully dirtside" << std::endl;
      stats.report_progess(child.z_);
    } else if (matching.empty()) {
      // std::cout << "found fully seaside" << std::endl;
      stats.report_progess(child.z_);
    } else if (child.z_ < 10) {
      // std::cout << "recursive descent" << std::endl;
      geo_q.enqueue(geo_task{child, std::move(matching)});
    } else {
      // std::cout << "save to database" << std::endl;
      db_q.enqueue({child, finalize_tile(clip, matching)});
      stats.report_progess(child.z_);
    }
  }
}

void load_coastlines(tile_db_handle& handle, std::string const& fname) {
  geo_queue_t geo_queue;
  db_queue_t db_queue;
  coastline_stats stats;

  auto convert_path = [](auto const& in) {
    return utl::to_vec(in, [](auto const& pt) {
      return cl::IntPoint{pt.x(), pt.y()};
    });
  };

  {
    std::vector<coastline_ptr> coastlines;
    auto coastline_handler = [&](fixed_simple_polygon geo) {
      cl::Paths coastline;
      coastline.emplace_back(convert_path(geo.outer()));
      for (auto const& inner : geo.inners()) {
        coastline.emplace_back(convert_path(inner));
      }
      coastlines.emplace_back(std::make_shared<struct coastline>(
          bounding_box(geo), std::move(coastline)));
    };
    load_shapefile(fname, coastline_handler);

    constexpr auto const kInitialZoomlevel = 4u;
    auto it = geo::tile_iterator(kInitialZoomlevel);
    while (it->z_ == kInitialZoomlevel) {
      geo_queue.enqueue(geo_task{*it, coastlines});
      ++it;
    }
  }

  // auto const num_workers = 1;
  auto const num_workers = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;
  for (auto i = 0u; i < num_workers; ++i) {
    threads.emplace_back([&] {
      while (!geo_queue.finished()) {
        geo_task task;
        if (!geo_queue.dequeue(task)) {
          continue;
        }

        process_coastline(task, geo_queue, db_queue, stats);
        geo_queue.finish();
      }
    });
  }

  {
    // FIXME map must be shared with other features inserter
    feature_inserter inserter{handle, &tile_db_handle::features_dbi};
    while (!geo_queue.finished() || !db_queue.finished()) {
      std::pair<geo::tile, std::string> data;
      if (!db_queue.dequeue(data)) {
        continue;
      }

      auto const idx = inserter.fill_state_[{data.first.x_, data.first.y_}]++;
      auto const key = make_feature_key(data.first, idx);

      inserter.insert(key, data.second);
      db_queue.finish();
    }
  }

  std::for_each(begin(threads), end(threads), [](auto& t) { t.join(); });

  verify(geo_queue.queue_.size_approx() == 0, "geo_queue not empty");
  verify(db_queue.queue_.size_approx() == 0, "db_queue not empty");
}

}  // namespace tiles
