#pragma once

#include "rocksdb/utilities/spatial_db.h"

#include "geo/webmercator.h"

#include "tiles/util.h"

namespace tiles {

constexpr auto kTileSize = 4096;
using proj = geo::webmercator<kTileSize, 20>;
constexpr auto kMaxZoomLevel = proj::kMaxZoomLevel;

constexpr auto kOverdraw = 128;

struct tile_spec {
  tile_spec(uint32_t const x, uint32_t const y, uint32_t const z)
      : x_(x), y_(y), z_(z), delta_z_(kMaxZoomLevel - z) {
    verify(delta_z_ >= 0 && delta_z_ <= kMaxZoomLevel, "invalid z");

    pixel_bounds_ = proj::tile_bounds_pixel(x_, y_); // lvl z

    bounds_ = pixel_bounds_; // lvl 20
    bounds_.minx_ = bounds_.minx_ << delta_z_;
    bounds_.miny_ = bounds_.miny_ << delta_z_;
    bounds_.maxx_ = bounds_.maxx_ << delta_z_;
    bounds_.maxy_ = bounds_.maxy_ << delta_z_;

    overdraw_bounds_ = bounds_;
    overdraw_bounds_.minx_ -= kOverdraw;
    overdraw_bounds_.miny_ -= kOverdraw;
    overdraw_bounds_.maxx_ += kOverdraw;
    overdraw_bounds_.maxy_ += kOverdraw;
  }

  std::string z_str() const {
    return "zoom10";  // TODO
  }

  rocksdb::spatial::BoundingBox<double> bbox() const {
    return {
        static_cast<double>(bounds_.minx_), static_cast<double>(bounds_.miny_),
        static_cast<double>(bounds_.maxx_), static_cast<double>(bounds_.maxy_)};
  }

  uint32_t x_, y_, z_, delta_z_;
  geo::pixel_bounds pixel_bounds_, bounds_, overdraw_bounds_;
};

}  // namespace tiles
