#pragma once

#include <memory>

#include "osmium/handler.hpp"

#include "tiles/db/feature_inserter.h"
#include "tiles/db/shared_strings.h"

namespace tiles {

struct feature_handler : public osmium::handler::Handler {
  feature_handler(feature_inserter&, layer_names_builder&);
  ~feature_handler();

  void node(osmium::Node const&);
  void way(osmium::Way const&);
  void area(osmium::Area const&);

private:
  struct script_runner;
  std::unique_ptr<script_runner> runner_;

  feature_inserter& inserter_;
  layer_names_builder& layer_names_builder_;
};

}  // namespace tiles
