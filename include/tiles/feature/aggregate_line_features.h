#pragma once

#include <vector>

namespace tiles {

struct feature;

std::vector<feature> aggregate_line_features(std::vector<feature>,
                                             uint32_t const z);

}  // namespace tiles
