#pragma once

namespace tiles {


void pack_features(tile_db_handle& handle) {
  feature_inserter inserter{handle, &tile_db_handle::features_dbi};

  auto feature_dbi = handle.features_dbi(inserter.txn_);
  auto c = lmdb::cursor{inserter.txn_, feature_dbi};

  for (auto const& tile : get_feature_range(c)) {
    std::vector<std::string> features;
     query_features(c, tile, [&](auto const& str) {
        features.push_back(str);
     });

     auto packed = pack_features(tile, features);


  }
}

} // namespace tiles
