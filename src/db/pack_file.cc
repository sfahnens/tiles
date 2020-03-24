#include "tiles/db/pack_file.h"

#include "tiles/lz4.h"

#include "tiles/util.h"

namespace tiles {

std::string pack_handle::get(pack_record record) const {
  utl::verify(record.offset_ < dat_.size() &&
                  record.offset_ + record.size_ <= dat_.size(),
              "pack_file: record not file [size={},record=({},{})", dat_.size(),
              record.offset_, record.size_);
  auto decompressed = lz4_decompress(
      std::string_view{dat_.data() + record.offset_, record.size_});
  return std::string{begin(decompressed), end(decompressed)};
}

pack_record pack_handle::insert(size_t offset, std::string_view dat) {
  auto compressed = lz4_compress(dat);
  pack_record record{offset, compressed.size()};
  dat_.resize(std::max(dat_.size(), record.offset_ + record.size_));
  std::memcpy(dat_.data() + offset, compressed.data(), compressed.size());
  return record;
}

}  // namespace tiles
