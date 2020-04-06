#include "catch.hpp"

#include "fmt/ostream.h"

#include "tiles/feature/aggregate_polygon_features.h"
#include "tiles/feature/feature.h"
#include "tiles/fixed/convert.h"

TEST_CASE("aggregate_polygon_features") {

  SECTION("two lakes") {
    tiles::feature cw_lake;
    cw_lake.id_ = 1;
    cw_lake.geometry_ = tiles::fixed_polygon{
        {{{tiles::latlng_to_fixed({49.3828501, 7.1518343}),
           tiles::latlng_to_fixed({49.3828500, 7.1518342}),
           tiles::latlng_to_fixed({49.3827526, 7.1519027}),
           tiles::latlng_to_fixed({49.3826395, 7.1519532}),
           tiles::latlng_to_fixed({49.3825794, 7.1519281}),
           tiles::latlng_to_fixed({49.3825585, 7.1516819}),
           tiles::latlng_to_fixed({49.3825619, 7.1514084}),
           tiles::latlng_to_fixed({49.3827291, 7.1515200}),
           tiles::latlng_to_fixed({49.3828677, 7.1516822}),
           tiles::latlng_to_fixed({49.3828500, 7.1518342}),
           tiles::latlng_to_fixed({49.3828501, 7.1518343})}}}};

    tiles::feature ccw_lake;
    ccw_lake.id_ = 2;
    ccw_lake.geometry_ = tiles::fixed_polygon{
        {{{tiles::latlng_to_fixed({49.3828500, 7.1519186}),
           tiles::latlng_to_fixed({49.3826652, 7.1520302}),
           tiles::latlng_to_fixed({49.3827317, 7.1523422}),
           tiles::latlng_to_fixed({49.3827274, 7.1526237}),
           tiles::latlng_to_fixed({49.3827459, 7.1527727}),
           tiles::latlng_to_fixed({49.3830321, 7.1527028}),
           tiles::latlng_to_fixed({49.3829186, 7.1522974}),
           tiles::latlng_to_fixed({49.3828500, 7.1519186})}}}};

    auto const result =
        tiles::aggregate_polygon_features({{cw_lake, ccw_lake}}, 7);

    REQUIRE(result.size() == 1);
    auto geo = mpark::get<tiles::fixed_polygon>(result.at(0).geometry_);
    REQUIRE(geo.size() == 1);
    CHECK(!geo.at(0).outer().empty());
    CHECK(geo.at(0).inners().empty());
  }

  // std::cout << "[\n";
  // for (auto const& pt : geo.at(0).outer()) {
  //   auto ll = tiles::fixed_to_latlng({pt.x(), pt.y()});
  //   fmt::print(std::cout, "[{}, {}],\n", ll.lng_, ll.lat_);
  // }
  // std::cout << "],\n";

  SECTION("four fields") {
    tiles::feature field_1;
    field_1.id_ = 1;
    field_1.geometry_ = tiles::fixed_polygon{
        {{tiles::latlng_to_fixed({49.7844041, 8.5676960}),
          tiles::latlng_to_fixed({49.7858957, 8.5740074}),
          tiles::latlng_to_fixed({49.7846715, 8.5745629}),
          tiles::latlng_to_fixed({49.7831512, 8.5682705}),
          tiles::latlng_to_fixed({49.7844041, 8.5676960})}}};
    tiles::feature field_2;
    field_2.id_ = 2;
    field_2.geometry_ = tiles::fixed_polygon{
        {{tiles::latlng_to_fixed({49.7871417, 8.5734467}),
          tiles::latlng_to_fixed({49.7876402, 8.5754229}),
          tiles::latlng_to_fixed({49.7819322, 8.5787253}),
          tiles::latlng_to_fixed({49.7813265, 8.5764393}),
          tiles::latlng_to_fixed({49.7825731, 8.5756850}),
          tiles::latlng_to_fixed({49.7841135, 8.5748812})}}};
    tiles::feature field_3;
    field_3.id_ = 3;
    field_3.geometry_ = tiles::fixed_polygon{
        {{tiles::latlng_to_fixed({49.7831214, 8.5683101}),
          tiles::latlng_to_fixed({49.7846381, 8.5745761}),
          tiles::latlng_to_fixed({49.7826542, 8.5755915}),
          tiles::latlng_to_fixed({49.7813085, 8.5763795}),
          tiles::latlng_to_fixed({49.7807104, 8.5740773}),
          tiles::latlng_to_fixed({49.7804036, 8.5729523}),
          tiles::latlng_to_fixed({49.7813132, 8.5685930}),
          tiles::latlng_to_fixed({49.7824894, 8.5684245}),
          tiles::latlng_to_fixed({49.7831214, 8.5683101})}}};
    tiles::feature field_4;
    field_4.id_ = 4;
    field_4.geometry_ = tiles::fixed_polygon{
        {{tiles::latlng_to_fixed({49.7856212, 8.5671224}),
          tiles::latlng_to_fixed({49.7871253, 8.5734085}),
          tiles::latlng_to_fixed({49.7859360, 8.5739825}),
          tiles::latlng_to_fixed({49.7844243, 8.5676848}),
          tiles::latlng_to_fixed({49.7856212, 8.5671224})}}};

    auto const result = tiles::aggregate_polygon_features(
        {{field_1, field_2, field_3, field_4}}, 12);

    REQUIRE(result.size() == 1);
    auto geo = mpark::get<tiles::fixed_polygon>(result.at(0).geometry_);

    for (auto const& polygon : geo) {
      std::cout << "[\n";
      for (auto pt : polygon.outer()) {
        auto ll = tiles::fixed_to_latlng({pt.x(), pt.y()});
        fmt::print(std::cout, "[{}, {}],\n", ll.lng_, ll.lat_);
      }
      std::cout << "],\n";

      for (auto const& r : polygon.inners()) {
        std::cout << "[\n";
        for (auto const& pt : r) {
          auto ll = tiles::fixed_to_latlng({pt.x(), pt.y()});
          fmt::print(std::cout, "[{}, {}],\n", ll.lng_, ll.lat_);
        }
        std::cout << "],\n";
      }
      std::cout << "------\n";
    }

    REQUIRE(geo.size() == 1);
    CHECK(!geo.at(0).outer().empty());
    CHECK(geo.at(0).inners().empty());
  }
}
