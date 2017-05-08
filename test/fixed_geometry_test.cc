#include "catch.hpp"

#include "tiles/fixed/deserialize.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/fixed/serialize.h"

using namespace tiles;

TEST_CASE("fixed point io") {
  std::vector<fixed_xy> test_cases;
  test_cases.emplace_back(kFixedCoordMin, kFixedCoordMin);
  test_cases.emplace_back(kFixedCoordMax, kFixedCoordMax);

  std::mt19937 gen{0};
  std::uniform_int_distribution<fixed_coord_t> dist{kFixedCoordMin,
                                                    kFixedCoordMax};
  for (auto i = 0u; i < 10000; ++i) {
    test_cases.emplace_back(dist(gen), dist(gen));
  }

  for (auto const& test_case : test_cases) {
    auto const serialized = serialize(test_case);
    auto const deserialized = deserialize(serialized);

    CHECK(test_case == boost::get<fixed_xy>(deserialized));
  }
}

TEST_CASE("fixed polyline io") {
  std::vector<fixed_polyline> test_cases;

  test_cases.push_back({{{10, 10}, {20, 20}}});
  test_cases.push_back({{{10, 10}, {10, 10}}});

  test_cases.push_back(
      {{{kFixedCoordMin, kFixedCoordMin}, {kFixedCoordMax, kFixedCoordMax}}});
  test_cases.push_back(
      {{{kFixedCoordMax, kFixedCoordMax}, {kFixedCoordMin, kFixedCoordMin}}});

  std::mt19937 gen{0};
  std::uniform_int_distribution<fixed_coord_t> coord_dist{kFixedCoordMin,
                                                          kFixedCoordMax};
  std::uniform_int_distribution<fixed_coord_t> len_dist{1, 10000};

  for (auto i = 0u; i < 1000; ++i) {
    auto len = len_dist(gen);

    fixed_polyline line;
    for (auto j = 0u; j < len; ++j) {
      line.geometry_.emplace_back(coord_dist(gen), coord_dist(gen));
    }
    test_cases.emplace_back(std::move(line));
  }

  for (auto const& test_case : test_cases) {
    auto const serialized = serialize(test_case);
    auto const deserialized = deserialize(serialized);

    CHECK(test_case.geometry_ ==
          boost::get<fixed_polyline>(deserialized).geometry_);
  }
}

// TEST_CASE("fixed geometry") {
//   SECTION("extremes") {
//     std::vector<fixed_xy> data{{fixed_coord_min, fixed_coord_min},
//                                {fixed_coord_max, fixed_coord_max}};

//     std::vector<std::string> lhs{"asdf", "bsdf", "csdf"};
//     std::vector<int> rhs{1, 2, 3};

//     auto const range = utl::zip(lhs, rhs);
//     auto it = begin(range);

//     {
//       REQUIRE(it != end(range));

//       auto[str, num] = *it;
//       CHECK(str == "asdf");
//       CHECK(num == 1);
//     }
//     {
//       ++it;
//       REQUIRE(it != end(range));

//       auto[str, num] = *it;
//       CHECK(str == "bsdf");
//       CHECK(num == 2);
//     }
//     {
//       ++it;
//       REQUIRE(it != end(range));

//       auto[str, num] = *it;
//       CHECK(str == "csdf");
//       CHECK(num == 3);
//     }

//     ++it;
//     REQUIRE(it == end(range));
//   }
// }
