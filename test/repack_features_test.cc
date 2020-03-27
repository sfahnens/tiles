#include "catch.hpp"

#include <random>

#include "utl/erase_if.h"

#include "tiles/db/repack_features.h"
#include "tiles/db/tile_index.h"

namespace tiles {

struct null_pack_handle {
  explicit null_pack_handle(size_t max_record_size)
      : buf_(max_record_size, '\0') {}

  [[nodiscard]] size_t size() const { return size_; }
  [[nodiscard]] size_t capacity() const { return capacity_; }

  void resize(size_t new_size) {
    size_ = new_size;
    capacity_ = std::max(capacity_, new_size);
  }

  std::string_view get(pack_record record) const {
    utl::verify(
        record.offset_ < size_ && record.offset_ + record.size_ <= size_,
        "pack_file: record not file [size={},record=({},{})", size_,
        record.offset_, record.size_);

    utl::verify(record.size_ < size_, "null_pack_handle: request to big");
    return std::string_view{buf_.data(), record.size_};
  }

  pack_record move(size_t offset, pack_record from_record) {
    pack_record to_record{offset, from_record.size_};
    resize(std::max(size_, to_record.offset_ + to_record.size_));
    return to_record;
  }

  pack_record insert(size_t offset, std::string_view dat) {
    pack_record record{offset, dat.size()};
    resize(std::max(size_, record.offset_ + record.size_));
    return record;
  }

  pack_record append(std::string_view dat) { return insert(size_, dat); }

  pack_record append(size_t dat_size) {
    pack_record record{size_, dat_size};
    resize(std::max(size_, record.offset_ + record.size_));
    return record;
  }

  std::string buf_;  // some backing memory which can be dereferenced
  size_t size_{0};
  size_t capacity_{0};
};

}  // namespace tiles

TEST_CASE("repack_features", "[!hide]") {
  constexpr size_t const kMaxRecordSize = 100 * 1024 * 1024;
  tiles::null_pack_handle handle(kMaxRecordSize);

  std::vector<tiles::tile_record> tasks((1 << tiles::kTileDefaultIndexZoomLvl) *
                                        (1 << tiles::kTileDefaultIndexZoomLvl));
  auto it = geo::tile_iterator{tiles::kTileDefaultIndexZoomLvl};
  for (auto i = 0ul; i < tasks.size(); ++i) {
    utl::verify(it->z_ == tiles::kTileDefaultIndexZoomLvl, "it broken");
    tasks[i].tile_ = *it;
    ++it;
  }
  utl::verify(it->z_ == tiles::kTileDefaultIndexZoomLvl + 1, "it broken");

  static std::mt19937_64 rand{123456};
  std::uniform_int_distribution<size_t> insert_dist(0, tasks.size() - 1);
  std::normal_distribution size_dist(10000., 10000.);

  size_t initial_packs = 0;
  while (handle.size() < 1024UL * 1024 * 1024 * 40) {
    size_t size = std::fabs(size_dist(rand));
    if (size > kMaxRecordSize / 100 || size == 0) {
      continue;
    }
    tasks[insert_dist(rand)].records_.emplace_back(handle.append(size));
    ++initial_packs;
  }
  std::cout << "initial packs: " << initial_packs << std::endl;
  std::shuffle(begin(tasks), end(tasks), rand);

  // --------------------------


  // using namespace tiles;

  // utl::erase_if(tasks, [](auto const& t) { return t.records_.empty(); });
  // for (auto& task : tasks) {
  //   std::sort(begin(task.records_), end(task.records_));
  // }

  // std::vector<owned_pack_record> tmp;
  // for (auto const& task : tasks) {
  //   for (auto const& record : task.records_) {
  //     tmp.emplace_back(&task, record);
  //   }
  // }

  // std::sort(begin(tmp), end(tmp), [](auto const& a, auto const& b) {
  //   return a.record_.offset_ < b.record_.offset_;
  // });
  // for (auto i = begin(tmp); i != end(tmp) && std::next(i) != end(tmp); ++i) {
  //   utl::verify(
  //       (i->record_.offset_ + i->record_.size_ ==
  //        std::next(i)->record_.offset_),
  //       "build_record_block_tree: have gaps (not implemented) {} + {} != {}",
  //       i->record_.offset_, i->record_.size_, std::next(i)->record_.offset_);
  // }

  // // prepare blocks
  // std::vector<std::unique_ptr<record_block_node>> nodes;
  // for (auto i = 0UL; i < tmp.size(); i += kTargetRecordBlockSize) {
  //   auto n = std::make_unique<record_block_node>();
  //   n->payload_ = std::make_unique<std::vector<owned_pack_record>>(
  //       std::next(begin(tmp), i),
  //       std::next(begin(tmp),
  //                 std::min(i + kTargetRecordBlockSize, tmp.size())));

  //   utl::verify(!n->payload_->empty(), "build_record_block_tree: no payload");
  //   n->record_count_ = n->payload_->size();
  //   n->min_offset_ = n->payload_->front().record_.offset_;
  //   nodes.emplace_back(std::move(n));
  // }

  // tiles::scoped_timer t{"make_record_block_tree"};
  // auto root_ = make_record_block_tree(std::move(nodes));
  // std::cout << root_->max_gap_ << " " << root_->min_offset_ << std::endl;

  tiles::repack_features(
      handle, tasks,
      [&](auto tile, auto const& packs) {
        static std::mt19937_64 rand{123456};
        static std::normal_distribution dist{1.1, 0.1};

        size_t size = std::accumulate(
            begin(packs), end(packs), 0ul,
            [](auto const& acc, auto const& p) { return acc + p.size(); });
        size *= std::fabs(dist(rand));
        size = std::min(size, kMaxRecordSize);
        return std::string(size, '\0');
      },
      [&](auto const& updates) {
        // std::cout << "updates " << updates.size() << std::endl;
      });
}

// TEST_CASE("update_gap_vec") {
//   using namespace tiles;

//   null_pack_handle nph{0};

//   SECTION("empty") {
//     repack_memory_manager<null_pack_handle> sut{nph, {}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 0);
//     CHECK(sut.gap_vec_.empty());
//   }

//   SECTION("single") {
//     repack_memory_manager<null_pack_handle> sut{nph, {}};
//     sut.gap_vec_ = {{0, 3}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 1);
//     CHECK(sut.gap_vec_.size() == 1);
//     CHECK(sut.gap_vec_.at(0) == tiles::pack_record(0, 3));
//   }

//   SECTION("two-concat") {
//     repack_memory_manager<null_pack_handle> sut{nph, {}};
//     sut.gap_vec_ = {{0, 3}, {3, 3}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 1);
//     CHECK(sut.gap_vec_.size() == 1);
//     CHECK(sut.gap_vec_.at(0) == tiles::pack_record(0, 6));
//   }

//   SECTION("two-sep") {
//     repack_memory_manager<null_pack_handle> sut{nph, {}};
//     sut.gap_vec_ = {{0, 2}, {4, 2}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 2);
//     CHECK(sut.gap_vec_.size() == 2);
//     CHECK(sut.gap_vec_.at(0) == tiles::pack_record(0, 2));
//     CHECK(sut.gap_vec_.at(1) == tiles::pack_record(4, 2));
//   }

//   SECTION("two-sep-concat") {
//     repack_memory_manager<null_pack_handle> sut{nph, {}};
//     sut.gap_vec_ = {{0, 2}, {4, 2}, {6, 2}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 2);
//     CHECK(sut.gap_vec_.size() == 2);
//     CHECK(sut.gap_vec_.at(0) == tiles::pack_record(0, 2));
//     CHECK(sut.gap_vec_.at(1) == tiles::pack_record(4, 4));
//   }

//   SECTION("single-zero") {
//     repack_memory_manager<null_pack_handle> sut{nph, {}};
//     sut.gap_vec_ = {{0, 0}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 0);
//     CHECK(sut.gap_vec_.empty());
//   }

//   SECTION("two-zero") {
//     repack_memory_manager<null_pack_handle> sut{nph, {}};
//     sut.gap_vec_ = {{0, 3}, {3, 0}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 1);
//     CHECK(sut.gap_vec_.size() == 1);
//     CHECK(sut.gap_vec_.at(0) == tiles::pack_record(0, 3));
//   }

//   SECTION("two-zero-b") {
//     repack_memory_manager<decltype(nph)> sut{nph, {}};
//     sut.gap_vec_ = {{3, 0}, {3, 8}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 1);
//     CHECK(sut.gap_vec_.size() == 1);
//     CHECK(sut.gap_vec_.at(0) == tiles::pack_record(3, 8));
//   }

//   SECTION("two-zero-sep") {
//     repack_memory_manager<decltype(nph)> sut{nph, {}};
//     sut.gap_vec_ = {{0, 3}, {4, 0}};
//     sut.update_gap_vec();
//     CHECK(sut.gap_count_ == 1);
//     CHECK(sut.gap_vec_.size() == 1);
//     CHECK(sut.gap_vec_.at(0) == tiles::pack_record(0, 3));
//   }
// }
