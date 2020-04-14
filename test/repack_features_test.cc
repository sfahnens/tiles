#include "catch2/catch.hpp"

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

  tiles::repack_features(
      handle, tasks,
      [&](auto, auto const& packs) {
        static std::mt19937_64 rand{123456};
        static std::normal_distribution dist{1.1, 0.1};

        size_t size = std::accumulate(
            begin(packs), end(packs), 0ul,
            [](auto const& acc, auto const& p) { return acc + p.size(); });
        size *= std::fabs(dist(rand));
        size = std::min(size, kMaxRecordSize);
        return std::string(size, '\0');
      },
      [&](auto const&) {
        // std::cout << "updates " << updates.size() << std::endl;
      });

  // TODO add some sanity checks
}
