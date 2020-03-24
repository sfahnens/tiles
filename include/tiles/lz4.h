#pragma once

#include "lz4frame.h"

#include <iostream>

#include "utl/parser/buffer.h"
#include "utl/raii.h"

#define LZ4F_EX(EXPR)                                   \
  ([&] {                                                \
    auto const ret = (EXPR);                            \
    if (LZ4F_isError(ret)) {                            \
      throw std::runtime_error{LZ4F_getErrorName(ret)}; \
    } else {                                            \
      return ret;                                       \
    }                                                   \
  })()

namespace tiles {

// std::string lz4_decompress(std::string_view input) {
//   uint8_t const* src_data = reinterpret_cast<uint8_t const*>(input.data());
//   size_t src_rest = input.size();

//   auto ctx = utl::make_raii<LZ4F_dctx*>(
//       nullptr, [](auto& ctx) { LZ4F_freeDecompressionContext(ctx); });
//   LZ4F_EX(LZ4F_createDecompressionContext(&ctx.get(), LZ4F_VERSION));

//   utl::verify(src_rest >= LZ4F_HEADER_SIZE_MAX, "cannot reader lz4f header");

//   LZ4F_frameInfo_t info;
//   size_t header_size = LZ4F_HEADER_SIZE_MAX;
//   LZ4F_EX(LZ4F_getFrameInfo(ctx, &info, src_data, &header_size));
//   src_data += header_size;
//   src_rest -= header_size;

//   utl::verify(info.contentSize > 0, "unknown contentSize is not supported");
//   std::string buf(info.contentSize, '\0');
//   uint8_t* result_data = reinterpret_cast<uint8_t*>(buf.data());
//   size_t result_rest = buf.size();

//   size_t ret = 1;
//   while (src_rest > 0 && ret != 0) {
//     auto in_size = src_rest;
//     auto out_size = result_rest;

//     ret = LZ4F_EX(LZ4F_decompress(ctx, result_data, &out_size,  //
//                                   src_data, &in_size, NULL));

//     result_data += out_size;
//     result_rest -= out_size;
//     src_data += in_size;
//     src_rest -= in_size;
//   }
//   utl::verify(src_rest == 0, "trailing data found");
//   utl::verify(result_rest == 0, "data missing");

//   return buf;
// }

// std::string lz4_compress(std::string_view input) {
//   LZ4F_preferences_t prefs{
//       LZ4F_frameInfo_t{LZ4F_max4MB, LZ4F_blockLinked, LZ4F_noContentChecksum,
//                        LZ4F_frame, input.size() /* decompressed content size
//                        */, 0 /* no dictID */, LZ4F_noBlockChecksum},
//       0, /* compression level; 0 == default */
//       0, /* autoflush */
//       0, /* favor decompression speed */
//       {0, 0, 0}, /* reserved, must be set to 0 */
//   };

//   auto ctx = utl::make_raii<LZ4F_compressionContext_t>(
//       {}, [](auto& ctx) { LZ4F_freeCompressionContext(ctx); });
//   LZ4F_EX(LZ4F_createCompressionContext(&ctx.get(), LZ4F_VERSION));

//   constexpr size_t kChunkSize = 64 * 1024;
//   utl::buffer buf{LZ4F_compressBound(kChunkSize, &prefs)};
//   utl::verify(buf.size() >= LZ4F_HEADER_SIZE_MAX, "LZ4F buffer too small.");

//   std::string output;
//   auto const write = [&](auto bytes) {
//     output.insert(end(output), std::begin(buf), std::begin(buf) + bytes);
//   };

//   write(LZ4F_EX(LZ4F_compressBegin(ctx, buf.data(), buf.size(), &prefs)));
//   for (auto offset = 0ul; offset < input.size(); offset += kChunkSize) {
//     write(LZ4F_EX(LZ4F_compressUpdate(
//         ctx, buf.data(), buf.size(), input.data() + offset,
//         std::min(kChunkSize, input.size() - offset), NULL)));
//   }
//   write(LZ4F_EX(LZ4F_compressEnd(ctx, buf.data(), buf.size(), NULL)));

//   return output;
// }

std::vector<char> lz4_decompress(std::string_view input) {
  uint8_t const* src_data = reinterpret_cast<uint8_t const*>(input.data());
  size_t src_rest = input.size();

  auto ctx = utl::make_raii<LZ4F_dctx*>(
      nullptr, [](auto& ctx) { LZ4F_freeDecompressionContext(ctx); });
  LZ4F_EX(LZ4F_createDecompressionContext(&ctx.get(), LZ4F_VERSION));

  utl::verify(src_rest >= LZ4F_HEADER_SIZE_MAX, "cannot reader lz4f header");

  LZ4F_frameInfo_t info;
  size_t header_size = LZ4F_HEADER_SIZE_MAX;
  LZ4F_EX(LZ4F_getFrameInfo(ctx, &info, src_data, &header_size));
  src_data += header_size;
  src_rest -= header_size;

  utl::verify(info.contentSize > 0, "unknown contentSize is not supported");
  std::vector<char> buf(info.contentSize);
  uint8_t* result_data = reinterpret_cast<uint8_t*>(buf.data());
  size_t result_rest = buf.size();

  size_t ret = 1;
  while (src_rest > 0 && ret != 0) {
    auto in_size = src_rest;
    auto out_size = result_rest;

    ret = LZ4F_EX(LZ4F_decompress(ctx, result_data, &out_size,  //
                                  src_data, &in_size, NULL));

    result_data += out_size;
    result_rest -= out_size;
    src_data += in_size;
    src_rest -= in_size;
  }
  utl::verify(src_rest == 0, "trailing data found");
  utl::verify(result_rest == 0, "data missing");

  return buf;
}

std::vector<char> lz4_compress(std::string_view input) {
  LZ4F_preferences_t prefs{
      LZ4F_frameInfo_t{LZ4F_max4MB, LZ4F_blockLinked, LZ4F_noContentChecksum,
                       LZ4F_frame, input.size() /* decompressed content size */,
                       0 /* no dictID */,
                       LZ4F_blockChecksumEnabled},  // LZ4F_noBlockChecksum},
      0, /* compression level; 0 == default */
      0, /* autoflush */
      0, /* favor decompression speed */
      {0, 0, 0}, /* reserved, must be set to 0 */
  };

  auto ctx = utl::make_raii<LZ4F_compressionContext_t>(
      {}, [](auto& ctx) { LZ4F_freeCompressionContext(ctx); });
  LZ4F_EX(LZ4F_createCompressionContext(&ctx.get(), LZ4F_VERSION));

  constexpr size_t kChunkSize = 64 * 1024;
  utl::buffer buf{LZ4F_compressBound(kChunkSize, &prefs)};
  utl::verify(buf.size() >= LZ4F_HEADER_SIZE_MAX, "LZ4F buffer too small.");

  std::vector<char> output;
  auto const write = [&](auto bytes) {
    output.insert(end(output), std::begin(buf), std::begin(buf) + bytes);
  };

  write(LZ4F_EX(LZ4F_compressBegin(ctx, buf.data(), buf.size(), &prefs)));
  for (auto offset = 0ul; offset < input.size(); offset += kChunkSize) {
    write(LZ4F_EX(LZ4F_compressUpdate(
        ctx, buf.data(), buf.size(), input.data() + offset,
        std::min(kChunkSize, input.size() - offset), NULL)));
  }
  write(LZ4F_EX(LZ4F_compressEnd(ctx, buf.data(), buf.size(), NULL)));

  return output;
}

}  // namespace tiles