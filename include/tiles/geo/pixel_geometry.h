#pragma once

namespace tiles {

using pixel32_t = uint32_t;
using pixel32_xy = geo::xy<pixel32_t>;

pixel32_xy latlng_to_pixel32(geo::latlng const& pos) {
  auto const px = proj::merc_to_pixel(latlng_to_merc(pos), proj::kMaxZoomLevel);
  constexpr int64_t kMax = std::numeric_limits<uint32_t>::max();
  return {static_cast<pixel32_t>(std::min(px.x_, kMax)),
          static_cast<pixel32_t>(std::min(px.y_, kMax))};
}



struct int_point {
  pixel32_xy point_;
};

struct pixel_polyline {
  std::vector<pixel32_t> polyline_;
};


using lvl20 = boost::variant<flat_point, flat, 

using localization =
    boost::variant<trip_localization, spatial_localization, coarse_localization,
                   ambigous_localization, failed_localization>;



std::vector<pixel_32_t> decode(std::string const& buffer) {


}

std::string encode(std::vector<pixel_32_t> const& polyline) {



}


} // namespace tiles
