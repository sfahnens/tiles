#pragma once

#include "geo/tile.h"

#include "tiles/db/feature_pack.h"
#include "tiles/feature/feature.h"

namespace tiles {

struct shared_metadata_coder;

struct quadtree_feature {
  quadtree_feature(std::vector<uint8_t> quad_key, geo::tile best_tile,
                   feature feature)
      : quad_key_{std::move(quad_key)},
        best_tile_{std::move(best_tile)},
        feature_{std::move(feature)} {}

  friend bool operator<(quadtree_feature const& a, quadtree_feature const& b) {
    return std::tie(a.quad_key_, a.best_tile_, a.feature_.id_) <
           std::tie(b.quad_key_, b.best_tile_, b.feature_.id_);
  }

  std::vector<uint8_t> quad_key_;
  geo::tile best_tile_;
  feature feature_;
};

struct quadtree_feature_packer {
  quadtree_feature_packer(geo::tile root,
                          shared_metadata_coder const& metadata_coder)
      : root_{root}, metadata_coder_{metadata_coder} {
    packer_.register_segment(kQuadTreeFeatureIndexId);
  }

  virtual ~quadtree_feature_packer() = default;

  void pack_features(std::vector<std::string> const& packs);

  geo::tile find_best_tile(feature const&) const;
  std::vector<uint8_t> make_quad_key(geo::tile const& tile) const;

  using quadtree_feature_it = std::vector<quadtree_feature>::iterator;
  virtual uint32_t serialize_and_append_span(quadtree_feature_it,
                                             quadtree_feature_it);

  void finish();

  geo::tile root_;
  shared_metadata_coder const& metadata_coder_;

  feature_packer packer_;
};

}  // namespace tiles
