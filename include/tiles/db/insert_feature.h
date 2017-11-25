#pragma once

#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/feature/feature.h"
#include "tiles/feature/serialize.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/convert.h"
#include "tiles/fixed/fixed_geometry.h"

namespace tiles {

void insert_feature(tile_database& tdb, feature const& f) {
  auto const box = bounding_box(f.geometry_);

  auto txn = lmdb::txn{tdb.env_};
  // auto db = txn.dbi_open(lmdb::dbi_flags::DUPSORT);
  auto db = txn.dbi_open();

  auto const value = serialize_feature(f);

  // std::cout << value.size() << std::endl;

  // auto a = fixed_to_latlng(box.min_corner());
  // auto b = fixed_to_latlng(box.max_corner());

  // std::cout.precision(11);
  // std::cout << a.lng_ << ", " << a.lat_ << std::endl;
  // std::cout << b.lng_ << ", " << b.lat_ << std::endl;


  uint32_t z = 10;  // whatever
  for (auto const& tile : make_tile_range(box, z)) {
    auto key = tile_to_key(tile);

    // std::cout << tile.x_ << ", " << tile.y_ << ", " << tile.z_ << std::endl;

    std::cout << std::to_string(key).size() << " " << value.size() << std::endl;

    txn.put(db, std::to_string(key), value);
  }
  txn.commit();
}

}  // namespace tiles
