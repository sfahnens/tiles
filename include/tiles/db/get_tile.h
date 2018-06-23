#pragma once

#include "geo/tile.h"
#include "lmdb/lmdb.hpp"

#include "tiles/db/bq_tree.h"
#include "tiles/db/query_features.h"
#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/feature/deserialize.h"
#include "tiles/mvt/tile_builder.h"
#include "tiles/mvt/tile_spec.h"


#include "boost/geometry.hpp"

namespace tiles {

struct render_ctx {
  int max_prepared_zoom_level_ = -1;
  bq_tree seaside_tiles_;
};

render_ctx make_render_ctx(tile_db_handle& handle) {
  auto txn = lmdb::txn{handle.env_};

  auto meta_dbi = handle.meta_dbi(txn);

  auto opt_max_prep = txn.get(meta_dbi, kMetaKeyMaxPreparedZoomLevel);
  auto opt_seaside = txn.get(meta_dbi, kMetaKeyFullySeasideTree);

  return {opt_max_prep ? std::stoi(std::string{*opt_max_prep}) : -1,
          opt_seaside ? bq_tree{*opt_seaside} : bq_tree{}};
}

std::string render_tile(lmdb::cursor& c, render_ctx const& ctx,
                        geo::tile const& tile) {
  tile_builder builder{tile};

  for (auto const& seaside_tile : ctx.seaside_tiles_.all_leafs(tile)) {
    auto const bounds = tile_spec{seaside_tile}.draw_bounds_;

    fixed_simple_polygon polygon{
        {{bounds.min_corner().x(), bounds.min_corner().y()},
         {bounds.min_corner().x(), bounds.max_corner().y()},
         {bounds.max_corner().x(), bounds.max_corner().y()},
         {bounds.max_corner().x(), bounds.min_corner().y()},
         {bounds.min_corner().x(), bounds.min_corner().y()}}};
    boost::geometry::correct(polygon);

    builder.add_feature({0ul,
                         std::pair<uint32_t, uint32_t>{0, kMaxZoomLevel + 1},
                         {{"layer", "coastline"}},
                         fixed_polygon{std::move(polygon)}});
  }

  query_features(c, tile, [&](auto const& str) {
    auto const feature = deserialize_feature(str, tile.z_);
    if (!feature.is_valid()) {
      return;
    }
    builder.add_feature(feature);
  });
  return builder.finish();
}

std::optional<std::string> get_tile(tile_db_handle& handle,
                                    lmdb::txn& txn,
                                    lmdb::cursor& features_cursor,
                                    render_ctx const& ctx,
                                    geo::tile const& tile) {
  if (static_cast<int>(tile.z_) <= ctx.max_prepared_zoom_level_) {
    auto tiles_dbi = handle.tiles_dbi(txn);
    auto db_tile = txn.get(tiles_dbi, make_tile_key(tile));
    if (!db_tile) {
      return std::nullopt;
    }
    return std::string{*db_tile};
  }

  auto const rendered_tile = render_tile(features_cursor, ctx, tile);
  if (rendered_tile.empty()) {
    return std::nullopt;
  }
  return compress_gzip(rendered_tile);
}

std::optional<std::string> get_tile(tile_db_handle& handle,
                                    render_ctx const& ctx,
                                    geo::tile const& tile) {
  lmdb::txn txn{handle.env_};
  auto features_dbi = handle.features_dbi(txn);
  auto features_cursor = lmdb::cursor{txn, features_dbi};

  return get_tile(handle, txn, features_cursor, ctx, tile);
}

}  // namespace tiles
