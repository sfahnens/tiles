#pragma once

#include <memory>

#include "tiles/loader/pending_feature.h"

namespace tiles {

struct script_runner {
  script_runner();
  ~script_runner();

  void process_node(pending_node&);
  void process_way(pending_way&);
  void process_relation(pending_relation&);

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
