#include "tiles/db/feature_pack.h"

#include <numeric>
#include <optional>

#include "boost/crc.hpp"

#include "utl/concat.h"
#include "utl/equal_ranges.h"
#include "utl/equal_ranges_linear.h"
#include "utl/erase_if.h"
#include "utl/to_vec.h"
#include "utl/verify.h"

#include "tiles/bin_utils.h"
#include "tiles/db/feature_pack_quadtree.h"
#include "tiles/db/pack_file.h"
#include "tiles/db/quad_tree.h"
#include "tiles/db/repack_features.h"
#include "tiles/db/shared_metadata.h"
#include "tiles/db/tile_database.h"
#include "tiles/db/tile_index.h"
#include "tiles/feature/deserialize.h"
#include "tiles/feature/feature.h"
#include "tiles/feature/serialize.h"
#include "tiles/fixed/algo/bounding_box.h"
#include "tiles/fixed/io/dump.h"
#include "tiles/mvt/tile_spec.h"
#include "tiles/util_parallel.h"

namespace tiles {

void feature_packer::finish() {
  boost::crc_32_type crc32;
  crc32.process_bytes(buf_.data(), buf_.size());
  tiles::append<uint32_t>(buf_, crc32.checksum());
}

bool feature_pack_valid(std::string_view const sv) {
  if (sv.size() < sizeof(uint32_t)) {
    return false;
  }
  boost::crc_32_type crc32;
  crc32.process_bytes(sv.data(), sv.size() - sizeof(uint32_t));
  return tiles::read<uint32_t>(sv.data(), sv.size() - sizeof(uint32_t)) ==
         crc32.checksum();
}

std::string pack_features(std::vector<std::string> const& serialized_features) {
  feature_packer p;
  p.finish_header(serialized_features.size());
  p.append_features(begin(serialized_features), end(serialized_features));
  p.finish();
  return p.buf_;
}

std::string pack_features(geo::tile const& tile,
                          shared_metadata_coder const& metadata_coder,
                          std::vector<std::string> const& packs) {
  quadtree_feature_packer p{tile, metadata_coder};
  p.pack_features(packs);
  p.finish();
  return p.packer_.buf_;
}

void pack_features(tile_db_handle& db_handle, pack_handle& pack_handle) {
  auto const metadata_coder = make_shared_metadata_coder(db_handle);
  pack_features(db_handle, pack_handle,
                [&](auto const tile, auto const& packs) {
                  return pack_features(tile, metadata_coder, packs);
                });
}

void pack_features(
    tile_db_handle& db_handle, pack_handle& pack_handle,
    std::function<std::string(geo::tile, std::vector<std::string> const&)>
        pack_fn) {
  std::vector<tile_record> tasks;
  {
    auto txn = db_handle.make_txn();
    auto feature_dbi = db_handle.features_dbi(txn);
    lmdb::cursor c{txn, feature_dbi};

    for (auto el = c.get<tile_index_t>(lmdb::cursor_op::FIRST); el;
         el = c.get<tile_index_t>(lmdb::cursor_op::NEXT)) {
      auto tile = feature_key_to_tile(el->first);
      auto records = pack_records_deserialize(el->second);
      utl::verify(!records.empty(), "pack_features: empty pack_records");

      if (!tasks.empty() && tasks.back().tile_ == tile) {
        utl::concat(tasks.back().records_, records);
      } else {
        tasks.push_back({tile, records});
      }
    }

    txn.dbi_clear(feature_dbi);
    txn.commit();
  }

  repack_features<std::string>(pack_handle, std::move(tasks),
                               std::move(pack_fn), [&](auto const& updates) {
                                 if (updates.empty()) {
                                   return;
                                 }

                                 auto txn = db_handle.make_txn();
                                 auto feature_dbi = db_handle.features_dbi(txn);
                                 for (auto const& [tile, records] : updates) {
                                   txn.put(feature_dbi, make_feature_key(tile),
                                           pack_records_serialize(records));
                                 }
                                 txn.commit();
                               });
}

}  // namespace tiles
