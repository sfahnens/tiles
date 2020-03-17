#pragma once

#include <memory>

#include "osmium/handler.hpp"

#include "tiles/db/feature_inserter_mt.h"
#include "tiles/db/shared_strings.h"

namespace tiles {

struct feature_handler : public osmium::handler::Handler {
  feature_handler(feature_inserter_mt&, layer_names_builder&);

  feature_handler(feature_handler&&) noexcept;
  feature_handler(feature_handler const&) = delete;
  feature_handler& operator=(feature_handler&&) = delete;
  feature_handler& operator=(feature_handler const&) = delete;

  ~feature_handler();

  void node(osmium::Node const&);
  void way(osmium::Way const&);
  void area(osmium::Area const&);

private:
  struct script_runner;
  std::unique_ptr<script_runner> runner_;

  feature_inserter_mt& inserter_;
  layer_names_builder& layer_names_builder_;
};

}  // namespace tiles
