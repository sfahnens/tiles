#pragma once

#include <optional>

#include "geo/tile.h"

#include "tiles/util.h"

namespace tiles {

template <typename RegexResult>
geo::tile url_match_to_tile(RegexResult const& rr) {
  utl::verify(rr.size() == 4, "url_match_to_tile: invalid input");
  return geo::tile{stou(rr[2]), stou(rr[3]), stou(rr[1])};
}

inline std::optional<geo::tile> parse_tile_url(std::string const& url) {
  static regex_matcher matcher{R"(.*\/(\d+)\/(\d+)\/(\d+).mvt$)"};
  auto match = matcher.match(url);
  if (!match) {
    return {};
  }

  return url_match_to_tile(*match);
}

}  // namespace tiles
