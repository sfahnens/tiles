#pragma once

#include <regex>
#include <optional>

#include "geo/tile.h"

namespace tiles {

inline std::optional<geo::tile> parse_tile_url(std::string const& url) {
  std::regex regex{"/(\\d+)\\/(\\d+)\\/(\\d+).mvt$"};
  std::smatch match;
  if (!std::regex_search(url, match, regex) || match.size() != 4) {
    return {};
  }

  return geo::tile{static_cast<uint32_t>(std::stoul(match[2])),
                   static_cast<uint32_t>(std::stoul(match[3])),
                   static_cast<uint32_t>(std::stoul(match[1]))};
}

}  // namespace tiles
