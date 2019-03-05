#include "tiles/util.h"

#include "utl/parser/util.h"

#include "zlib.h"

namespace tiles {

std::string compress_gzip(std::string const& input) {
  auto out_size = compressBound(input.size());
  std::string buffer('\0', out_size);

  auto error = compress2(reinterpret_cast<uint8_t*>(&buffer[0]), &out_size,
                         reinterpret_cast<uint8_t const*>(&input[0]),
                         input.size(), Z_BEST_COMPRESSION);
  verify(!error, "compress_gzip failed");

  buffer.resize(out_size);
  return buffer;
}

}  // namespace tiles
