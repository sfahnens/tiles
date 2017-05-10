#pragma once

#include "geo/webmercator.h"

namespace tiles {

constexpr auto kTileSize = 4096;
using proj = geo::webmercator<kTileSize, 20>;
constexpr auto kMaxZoomLevel = proj::kMaxZoomLevel;

struct tile_spec {
  tile_spec(uint32_t const x, uint32_t const y, uint32_t const z)
      : x_(x), y_(y), z_(z) {
    merc_bounds_ = proj::tile_bounds_merc(x_, y_, z_);
    pixel_bounds_ = proj::tile_bounds_pixel(x_, y_, z_);
  }

  std::string z_str() const {
    return "zoom10";  // TODO
  }

  uint32_t x_, y_, z_;
  geo::merc_bounds merc_bounds_;
  geo::pixel_bounds pixel_bounds_;
};

}  // namespace tiles
