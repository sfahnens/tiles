#include "catch.hpp"

#include "tiles/db/bq_tree.h"

TEST_CASE("bq_tree_contains") {
  SECTION("default ctor") {
    tiles::bq_tree tree;
    CHECK(1 == tree.nodes_.size());
    CHECK(false == tree.contains({0, 0, 0}));
  }

  SECTION("root tree") {
    auto empty_tree = tiles::make_bq_tree({});
    CHECK(1 == empty_tree.nodes_.size());
    CHECK(false == empty_tree.contains({0, 0, 0}));

    auto root_tree = tiles::make_bq_tree({{0, 0, 0}});
    CHECK(1 == root_tree.nodes_.size());
    CHECK(true == root_tree.contains({0, 0, 0}));
  }

  SECTION("l1 tree") {
    auto tree = tiles::make_bq_tree({{0, 0, 1}});
    CHECK(1 == tree.nodes_.size());

    // self
    CHECK(true == tree.contains({0, 0, 1}));

    // parent
    CHECK(false == tree.contains({0, 0, 0}));

    // siblings
    CHECK(false == tree.contains({0, 1, 1}));
    CHECK(false == tree.contains({1, 0, 1}));
    CHECK(false == tree.contains({1, 1, 1}));

    // child
    CHECK(true == tree.contains({0, 0, 2}));
  }

  SECTION("l2 tree") {
    auto tree = tiles::make_bq_tree({{0, 1, 2}, {3, 3, 2}});

    CHECK(3 == tree.nodes_.size());

    // self
    CHECK(true == tree.contains({0, 1, 2}));
    CHECK(true == tree.contains({3, 3, 2}));

    // root
    CHECK(false == tree.contains({0, 0, 0}));

    // parent
    CHECK(false == tree.contains({0, 0, 1}));
    CHECK(false == tree.contains({0, 1, 1}));
    CHECK(false == tree.contains({1, 0, 1}));
    CHECK(false == tree.contains({1, 1, 1}));

    // sibling
    CHECK(false == tree.contains({0, 0, 2}));
    CHECK(false == tree.contains({42, 48, 8}));
  }
}

TEST_CASE("bq_tree_all_leafs") {
  SECTION("default ctor") {
    tiles::bq_tree tree;
    CHECK(true == tree.all_leafs({0, 0, 0}).empty());
  }

  SECTION("root tree") {
    auto empty_tree = tiles::make_bq_tree({});
    CHECK(true == empty_tree.all_leafs({0, 0, 0}).empty());

    auto root_tree = tiles::make_bq_tree({{0, 0, 0}});
    auto root_result = root_tree.all_leafs({0, 0, 0});
    REQUIRE(1 == root_result.size());
    CHECK((geo::tile{0, 0, 0} == root_result.at(0)));
  }

  SECTION("l1 tree") {
    auto tree = tiles::make_bq_tree({{1, 1, 1}});
    {  // parent
      auto result = tree.all_leafs({0, 0, 0});
      REQUIRE(1 == result.size());
      CHECK((geo::tile{1, 1, 1} == result.at(0)));
    }
    {  // self
      auto result = tree.all_leafs({1, 1, 1});
      REQUIRE(1 == result.size());
      CHECK((geo::tile{1, 1, 1} == result.at(0)));
    }
    {  // sibling
      REQUIRE(true == tree.all_leafs({0, 0, 1}).empty());
      REQUIRE(true == tree.all_leafs({0, 1, 1}).empty());
      REQUIRE(true == tree.all_leafs({1, 0, 1}).empty());
    }
    {  // self - child
      auto result = tree.all_leafs({2, 2, 2});
      REQUIRE(1 == result.size());
      CHECK((geo::tile{2, 2, 2} == result.at(0)));
    }
    {  // self - child
      REQUIRE(true == tree.all_leafs({0, 0, 2}).empty());
    }
  }

  SECTION("l2 tree") {
    auto tree = tiles::make_bq_tree({{0, 1, 2}, {3, 3, 2}});
    {  // root
      auto result = tree.all_leafs({0, 0, 0});
      std::sort(begin(result), end(result));
      REQUIRE(2 == result.size());
      CHECK((geo::tile{0, 1, 2} == result.at(0)));
      CHECK((geo::tile{3, 3, 2} == result.at(1)));
    }
    {  // parent
      auto result = tree.all_leafs({0, 0, 1});
      std::sort(begin(result), end(result));
      REQUIRE(1 == result.size());
      CHECK((geo::tile{0, 1, 2} == result.at(0)));
    }
    {  // other
      REQUIRE(true == tree.all_leafs({0, 0, 8}).empty());
      REQUIRE(true == tree.all_leafs({42, 48, 8}).empty());
    }
  }
}
