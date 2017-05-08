#pragma once

#include <climits>

namespace tiles {

using fixed_delta_t = int32_t;

struct delta_encoder {
  delta_encoder(fixed_coord_t init) : curr_(init) {}

  fixed_delta_t encode(fixed_coord_t val) {
    // implementation defined: works as expected in clang and gcc
    auto delta = static_cast<fixed_delta_t>(val - curr_);
    curr_ = val;
    return delta;
  }

  fixed_coord_t curr_;
};

struct delta_decoder {
  delta_decoder(fixed_coord_t init) : curr_(init) {}

  fixed_coord_t decode(fixed_delta_t val) {
    curr_ += val;
    return curr_;
  }

  fixed_coord_t curr_;
};

}  // namespace tiles