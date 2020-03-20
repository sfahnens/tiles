#pragma once

#include <cstdint>

namespace tiles {

struct tile_db_handle;
struct pack_handle;

void prepare_tiles(tile_db_handle&, pack_handle&, uint32_t max_zoomlevel);

}  // namespace tiles
