#include "tiles/tile_database.h"

#include "utl/to_vec.h"
#include "utl/zip.h"

namespace fs = boost::filesystem;
namespace r = rocksdb;
namespace rs = rocksdb::spatial;

namespace tiles {

constexpr auto kPrepTilesMaxZoomKey = "__max_zoom_level";

void create_database(tile_database const& tile_db, std::string const& path,
                     std::vector<r::ColumnFamilyDescriptor> const& cf_descs) {
  auto const box =
      rs::BoundingBox<double>{0, 0, proj::map_size(20), proj::map_size(20)};

  std::vector<rs::SpatialIndexOptions> idx_options;
  for (auto i = 0u; i < tile_spec::zoom_level_bases().size(); ++i) {
    idx_options.emplace_back(tile_spec::zoom_level_names().at(i), box,
                             tile_spec::zoom_level_bases().at(i));
  }

  tile_db.verify_status(rs::SpatialDB::Create(rs::SpatialDBOptions(), path,
                                              idx_options, cf_descs));
}
std::unique_ptr<tile_database> make_tile_database(
    std::string const& path, bool read_only, bool truncate,
    std::vector<cf_holder*> const& extra_cfs,
    tile_database::error_handler_t error_handler) {
  auto tile_db = std::make_unique<tile_database>(std::move(error_handler));

  auto cfs = extra_cfs;  // deliberate copy!
  cfs.emplace_back(&tile_db->prep_tiles_cf_);

  auto const& cf_descs = utl::to_vec(cfs, [](auto&& cf) { return cf->desc_; });

  if (truncate && fs::is_directory(path)) {
    fs::remove_all(path);
  }

  if (!read_only && !fs::is_directory(path)) {
    create_database(*tile_db, path, cf_descs);
  }

  rs::SpatialDB* db;
  std::vector<r::ColumnFamilyHandle*> cf_handles;

  tile_db->verify_status(rs::SpatialDB::Open(rs::SpatialDBOptions{}, path, &db,
                                             cf_descs, &cf_handles, read_only));

  tile_db->db_.reset(db);

  for (auto const& tup : utl::zip(cfs, cf_handles)) {
    std::get<0>(tup)->handle_.reset(std::get<1>(tup));
  }

  std::string prep_max_zoom_level;
  auto status =
      tile_db->db_->Get(r::ReadOptions(), tile_db->prep_tiles_cf_.handle_.get(),
                        kPrepTilesMaxZoomKey, &prep_max_zoom_level);
  if (!status.IsNotFound()) {
    tile_db->verify_status(status);
    tile_db->prep_max_zoom_level_ = std::stoi(prep_max_zoom_level);
  }

  return tile_db;
}

void tile_database::put_feature(
    rs::BoundingBox<double> const& bbox, r::Slice const& slice,
    rs::FeatureSet const& feature,
    std::vector<std::string> const& spatial_indexes) {
  verify_status(
      db_->Insert(r::WriteOptions(), bbox, slice, feature, spatial_indexes));
}

std::string tile_database::get_tile(
    tile_spec const& spec, tile_builder::config const& tb_config) const {

  if (prep_max_zoom_level_ != kInvalidPrepMaxZoomLevel &&
      spec.z_ <= prep_max_zoom_level_) {
    std::string v;
    auto status = db_->Get(r::ReadOptions(), prep_tiles_cf_.handle_.get(),
                           spec.rocksdb_quad_key(), &v);

    if (status.IsNotFound()) {
      return "";
    } else {
      verify_status(status);
    }

    return v;
  }

  tile_builder builder{spec, tb_config};

  std::unique_ptr<rs::Cursor> cursor{
      db_->Query(r::ReadOptions(), spec.bbox(), spec.z_str())};
  while (cursor->Valid()) {
    builder.add_feature(cursor->feature_set(), cursor->blob());
    cursor->Next();
  }

  return builder.finish();
}

void tile_database::prepare_tiles(uint32_t max_z) {
  for (auto const& spec : tile_pyramid{}) {
    if (spec.z_ > max_z) {
      break;
    }

    auto tile = get_tile(spec);

    if (tile.empty()) {
      continue;
    }

    verify_status(db_->Put(r::WriteOptions(), prep_tiles_cf_.handle_.get(),
                           spec.rocksdb_quad_key(), tile));
  }

  verify_status(db_->Put(r::WriteOptions(), prep_tiles_cf_.handle_.get(),
                         kPrepTilesMaxZoomKey, std::to_string(max_z)));
}

void tile_database::compact(int num_threads) {
  verify_status(db_->Compact(num_threads));
}

}  // namespace tiles
