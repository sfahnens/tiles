#pragma once

#include <memory>

#include "osmium/handler.hpp"

#include "tiles/tile_database.h"

namespace tiles {

struct feature_handler : public osmium::handler::Handler {
  feature_handler(tile_database&);
  ~feature_handler();

  void node(osmium::Node const&);
  void way(osmium::Way const&);
  void area(osmium::Area const&);

private:
  struct script_runner;
  std::unique_ptr<script_runner> runner_;

  tile_database& db_;
};

}  // namespace tiles
