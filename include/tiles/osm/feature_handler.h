#pragma once

#include <memory>

#include "osmium/handler.hpp"

#include "tiles/db/feature_inserter.h"

namespace tiles {

struct feature_handler : public osmium::handler::Handler {
  feature_handler(feature_inserter&);
  ~feature_handler();

  void node(osmium::Node const&);
  void way(osmium::Way const&);
  void area(osmium::Area const&);

private:
  struct script_runner;
  std::unique_ptr<script_runner> runner_;

  feature_inserter& inserter_;
};

}  // namespace tiles
