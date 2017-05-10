#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

int const fixed_geometry_index::null =
    fixed_geometry{fixed_null_geometry{}}.which();

}  // namespace tiles
