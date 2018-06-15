#include "catch.hpp"

#include "tiles/db/bq_tree.h"

TEST_CASE("bq_tree") {

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
