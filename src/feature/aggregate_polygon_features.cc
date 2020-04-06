#include "tiles/feature/aggregate_polygon_features.h"

#include "utl/equal_ranges_linear.h"
#include "utl/to_vec.h"

#include "tiles/feature/feature.h"
#include "tiles/fixed/algo/simplify.h"
#include "tiles/fixed/clipper.h"
#include "tiles/fixed/convert.h"
#include "tiles/util.h"

namespace cl = ClipperLib;

namespace tiles {

std::vector<feature> aggregate_polygon_features(std::vector<feature> features,
                                                uint32_t const z) {
  std::sort(
      begin(features), end(features), [](auto const& lhs, auto const& rhs) {
        return std::tie(lhs.meta_, lhs.id_) < std::tie(rhs.meta_, rhs.id_);
      });

  double const delta = 16. * (1UL << (kMaxZoomLevel - z));

  std::vector<feature> result;
  utl::equal_ranges_linear(
      features,
      [](auto const& lhs, auto const& rhs) { return lhs.meta_ == rhs.meta_; },
      [&](auto lb, auto ub) {
        scoped_timer t{std::string{"aggregate_polygon_features_v2 "} +
                       std::to_string(std::distance(lb, ub))};

        cl::Paths offset_paths;
        for (auto it = lb; it != ub; ++it) {
          cl::Paths input;
          to_clipper_paths(mpark::get<fixed_polygon>(it->geometry_), input);

          cl::ClipperOffset co;
          co.AddPaths(input, cl::jtMiter, cl::etClosedPolygon);

          cl::Paths offset_solution;
          co.Execute(offset_solution, delta);

          for (auto& p : offset_solution) {
            offset_paths.emplace_back(std::move(p));
          }
        }

        cl::Clipper clpr;
        utl::verify(clpr.AddPaths(offset_paths, cl::ptSubject, true),
                    "add offset_paths failed");

        cl::PolyTree union_result;
        clpr.Execute(cl::ctUnion, union_result, cl::pftNonZero, cl::pftNonZero);

        fixed_polygon final_polygon;
        std::function<void(cl::PolyNodes const&)> reduce;
        reduce = [&](cl::PolyNodes const& nodes) {
          for (auto const* outer : nodes) {
            utl::verify(!outer->IsHole(), "outer ring is hole");

            cl::Paths paths;
            paths.emplace_back(std::move(outer->Contour));
            for (auto const* inner : outer->Childs) {
              utl::verify(inner->IsHole(), "inner ring is no hole");
              paths.emplace_back(std::move(inner->Contour));
              reduce(inner->Childs);
            }

            cl::ClipperOffset co;
            co.AddPaths(paths, cl::jtMiter, cl::etClosedPolygon);

            cl::PolyTree inset_solution;
            try {
              co.Execute(inset_solution, -delta);
            } catch (...) {
              std::cout << to_geojson(paths);
              throw;
            }

            if (inset_solution.Childs.size() > 0) {
              to_fixed_polygon(inset_solution.Childs, final_polygon);
            }
          }
        };

        reduce(union_result.Childs);

        feature f;
        f.id_ = lb->id_;
        f.meta_ = std::move(lb->meta_);

        f.geometry_ =
            simplify(std::move(final_polygon), 1UL << (kMaxZoomLevel - z));

        result.emplace_back(std::move(f));
      });

  return result;
}

}  // namespace tiles
