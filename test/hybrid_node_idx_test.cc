#include "catch.hpp"

#include "osmium/index/detail/tmpfile.hpp"
#include "osmium/io/pbf_input.hpp"
#include "osmium/io/reader_iterator.hpp"

#include "tiles/fixed/convert.h"
#include "tiles/osm/hybrid_node_idx.h"
#include "tiles/util.h"

#define CHECK_EXISTS(nodes, id, pos_x, pos_y)  \
  {                                            \
    auto const result = get_coords(nodes, id); \
    REQUIRE(result);                           \
    CHECK(pos_x == result->x());               \
    CHECK(pos_y == result->y());               \
  }

TEST_CASE("hybrid_node_idx") {

  SECTION("null") {
    tiles::hybrid_node_idx nodes;
    CHECK_FALSE(get_coords(nodes, 0));
  }

  SECTION("empty idx") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(get_coords(nodes, 0));
  }

  SECTION("entry single") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.push(42, {2, 3});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(get_coords(nodes, 0));
    CHECK_FALSE(get_coords(nodes, 100));

    CHECK_EXISTS(nodes, 42, 2, 3);
  }

  SECTION("entries consecutive") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.push(42, {2, 3});
      builder.push(43, {5, 6});
      builder.push(44, {8, 9});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(get_coords(nodes, 0));
    CHECK_FALSE(get_coords(nodes, 100));

    CHECK_EXISTS(nodes, 42, 2, 3);
    CHECK_EXISTS(nodes, 43, 5, 6);
    CHECK_EXISTS(nodes, 44, 8, 9);
  }

  SECTION("entries gap") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.push(42, {2, 3});
      builder.push(44, {8, 9});
      builder.push(45, {1, 2});
      builder.finish();
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(get_coords(nodes, 0));
    CHECK_FALSE(get_coords(nodes, 100));

    CHECK_FALSE(get_coords(nodes, 41));
    CHECK_FALSE(get_coords(nodes, 43));
    CHECK_FALSE(get_coords(nodes, 46));

    CHECK_EXISTS(nodes, 42, 2, 3);
    CHECK_EXISTS(nodes, 44, 8, 9);
    CHECK_EXISTS(nodes, 45, 1, 2);
  }

  SECTION("artificial splits") {
    auto const idx_fd = osmium::detail::create_tmp_file();
    auto const dat_fd = osmium::detail::create_tmp_file();

    {
      tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};
      builder.push(42, {2, 3});
      builder.push(43, {2, 7});
      builder.push(44, {(1 << 28) + 14, (1 << 28) + 15});
      builder.push(45, {(1 << 28) + 16, (1 << 28) + 17});
      builder.finish();

      CHECK(2 == builder.get_stat_spans());
    }

    tiles::hybrid_node_idx nodes{idx_fd, dat_fd};
    CHECK_FALSE(get_coords(nodes, 0));
    CHECK_FALSE(get_coords(nodes, 100));

    CHECK_FALSE(get_coords(nodes, 41));
    CHECK_FALSE(get_coords(nodes, 46));

    CHECK_EXISTS(nodes, 42, 2, 3);
    CHECK_EXISTS(nodes, 43, 2, 7);
    CHECK_EXISTS(nodes, 44, (1 << 28) + 14, (1 << 28) + 15);
    CHECK_EXISTS(nodes, 45, (1 << 28) + 16, (1 << 28) + 17);
  }
}

TEST_CASE("hybrid_node_idx_benchmark", "[!hide]") {
  tiles::t_log("start");

  auto const idx_fd = osmium::detail::create_tmp_file();
  auto const dat_fd = osmium::detail::create_tmp_file();
  tiles::hybrid_node_idx_builder builder{idx_fd, dat_fd};

  osmium::io::Reader reader("/data/osm/planet-latest.osm.pbf",
                            osmium::osm_entity_bits::node);
  for (auto it = osmium::io::begin(reader); it != osmium::io::end(reader);
       ++it) {
    auto const& node = static_cast<osmium::Node&>(*it);  // NOLINT

    builder.push(node.id(), tiles::latlng_to_fixed({node.location().lat(),
                                                    node.location().lon()}));
  }
  builder.finish();

  builder.dump_stats();
}
