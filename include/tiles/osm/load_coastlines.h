#pragma once

#include <string>

namespace tiles {

struct tile_db_handle;
struct feature_inserter_mt;

void load_coastlines(tile_db_handle&, feature_inserter_mt&,
                     std::string const& fname);

}  // namespace tiles
