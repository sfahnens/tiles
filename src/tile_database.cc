#include "tiles/tile_database.h"

#include "utl/to_vec.h"
#include "utl/zip.h"

namespace fs = boost::filesystem;
namespace r = rocksdb;
namespace rs = rocksdb::spatial;

namespace tiles {

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

  return tile_db;
}

void tile_database::put_feature(
    rs::BoundingBox<double> const& bbox, r::Slice const& slice,
    rs::FeatureSet const& feature,
    std::vector<std::string> const& spatial_indexes) {
  verify_database_open();

  verify_status(
      db_->Insert(r::WriteOptions(), bbox, slice, feature, spatial_indexes));
}

void tile_database::put_tile(tile_spec const&, r::Slice const&) {
  // TODO prepared tile stuff
}

std::string tile_database::get_tile(
    tile_spec const& spec, tile_builder::config const& tb_config) const {
  verify_database_open();

  tile_builder builder{spec, tb_config};

  std::unique_ptr<rs::Cursor> cursor{
      db_->Query(r::ReadOptions(), spec.bbox(), spec.z_str())};
  while (cursor->Valid()) {
    builder.add_feature(cursor->feature_set(), cursor->blob());
    cursor->Next();
  }

  return builder.finish();
}

void tile_database::compact(int num_threads) {
  verify_status(db_->Compact(num_threads));
}

}  // namespace tiles
