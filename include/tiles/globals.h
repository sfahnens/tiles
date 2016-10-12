#pragma once

#include "geo/webmercator.h"

namespace tiles {

constexpr auto kTileSize = 4096;
using proj = geo::webmercator<4096>;


} // tiles
