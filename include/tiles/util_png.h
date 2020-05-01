#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "boost/crc.hpp"

#include "utl/verify.h"

#include "tiles/bin_utils.h"
#include "tiles/util.h"

namespace tiles {

struct bitmap {
  struct pixel {
    pixel() = default;
    pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        : r_{r}, g_{g}, b_{b}, a_{a} {}
    explicit pixel(uint32_t value)
        : r_{static_cast<uint8_t>((value >> 24) & 0xFF)},
          g_{static_cast<uint8_t>((value >> 16) & 0xFF)},
          b_{static_cast<uint8_t>((value >> 8) & 0xFF)},
          a_{static_cast<uint8_t>((value >> 0) & 0xFF)} {}

    uint8_t r_, g_, b_, a_;
  };

  bitmap(size_t w, size_t h) : w_{w}, h_{h}, data_(w * h) {}

  pixel const& operator()(size_t x, size_t y) const {
    return data_[y * w_ + x];
  }
  pixel& operator()(size_t x, size_t y) { return data_[y * w_ + x]; }

  size_t w_, h_;
  std::vector<pixel> data_;
};

void append_chunk(std::string& buf, char const* type, std::string data) {
  utl::verify(data.size() <= std::numeric_limits<uint32_t>::max(),
              "append_chunk: data to large");
  append_network_byte_order32(buf, data.size());

  auto const size_pre = buf.size();

  buf.append(type, 4);
  buf.append(data.data(), data.size());

  boost::crc_32_type crc32;
  crc32.process_bytes(buf.data() + size_pre, buf.size() - size_pre);
  append_network_byte_order32(buf, crc32.checksum());
}

std::string encode_bitmap(bitmap const& img) {
  auto const filter_sub = [&](auto const y, std::string& out) {
    utl::verify(out.size() == 4 * img.w_, "invalid buffer size");
    for (auto x = 0ULL; x < img.w_; ++x) {
      auto left = x == 0 ? bitmap::pixel{0} : img(x - 1, y);
      auto curr = img(x, y);

      out[x * 4 + 0] = curr.r_ - left.r_;
      out[x * 4 + 1] = curr.g_ - left.g_;
      out[x * 4 + 2] = curr.b_ - left.b_;
      out[x * 4 + 3] = curr.a_ - left.a_;
    }
  };

  auto const filter_up = [&](auto const y, std::string& out) {
    utl::verify(y > 0, "cannot filter the 0th scanline");
    utl::verify(out.size() == 4 * img.w_, "invalid buffer size");
    for (auto x = 0ULL; x < img.w_; ++x) {
      auto top = img(x, y - 1);
      auto curr = img(x, y);

      out[x * 4 + 0] = curr.r_ - top.r_;
      out[x * 4 + 1] = curr.g_ - top.g_;
      out[x * 4 + 2] = curr.b_ - top.b_;
      out[x * 4 + 3] = curr.a_ - top.a_;
    }
  };

  auto const filter_avg = [&](auto const y, std::string& out) {
    utl::verify(y > 0, "cannot filter the 0th scanline");
    utl::verify(out.size() == 4 * img.w_, "invalid buffer size");
    for (auto x = 0ULL; x < img.w_; ++x) {
      auto left = x == 0 ? bitmap::pixel{0} : img(x - 1, y);
      auto top = img(x, y - 1);
      auto curr = img(x, y);

      out[x * 4 + 0] = curr.r_ - (left.r_ + top.r_) / 2;
      out[x * 4 + 1] = curr.g_ - (left.g_ + top.g_) / 2;
      out[x * 4 + 2] = curr.b_ - (left.b_ + top.b_) / 2;
      out[x * 4 + 3] = curr.a_ - (left.a_ + top.a_) / 2;
    }
  };

  auto const filter_paeth = [&](auto const y, std::string& out) {
    utl::verify(y > 0, "cannot filter the 0th scanline");
    utl::verify(out.size() == 4 * img.w_, "invalid buffer size");

    auto const paeth_predictor = [](int a, int b, int c) -> uint8_t {
      int p = a + b - c;
      int pa = std::abs(p - a);
      int pb = std::abs(p - b);
      int pc = std::abs(p - c);
      if (pa <= pb && pa <= pc) {
        return a;
      } else if (pb <= pc) {
        return b;
      } else {
        return c;
      }
    };

    for (auto x = 0ULL; x < img.w_; ++x) {
      auto topleft = x == 0 ? bitmap::pixel{0} : img(x - 1, y - 1);
      auto left = x == 0 ? bitmap::pixel{0} : img(x - 1, y);
      auto top = img(x, y - 1);
      auto curr = img(x, y);

      out[x * 4 + 0] = curr.r_ - paeth_predictor(left.r_, top.r_, topleft.r_);
      out[x * 4 + 1] = curr.g_ - paeth_predictor(left.g_, top.g_, topleft.g_);
      out[x * 4 + 2] = curr.b_ - paeth_predictor(left.b_, top.b_, topleft.b_);
      out[x * 4 + 3] = curr.a_ - paeth_predictor(left.a_, top.a_, topleft.a_);
    }
  };

  auto const compute_heuristic = [](auto const* data, auto const size) {
    int64_t counter = 0;
    for (auto i = 0ULL; i < size; ++i) {
      counter += std::abs(static_cast<int8_t>(data[i]));
    }
    return counter;
  };

  // result size: 4 bytes per pixel, one filter type per scanline
  std::string filtered;
  filtered.reserve(img.w_ * img.w_ * 4 + img.h_);

  std::string best_scanline(img.w_ * sizeof(uint32_t), '\0');
  std::string curr_scanline(img.w_ * sizeof(uint32_t), '\0');
  for (auto y = 0UL; y < img.h_; ++y) {
    uint8_t best_filter = 0;
    int64_t best_heuristic = compute_heuristic(
        reinterpret_cast<int8_t const*>(&img(0, y)), img.w_ * 4);

    auto const applyFilterAndUpdateBest = [&](auto filter_idx,
                                              auto& filter_fn) {
      filter_fn(y, curr_scanline);
      auto const curr_heuristic =
          compute_heuristic(curr_scanline.data(), curr_scanline.size());

      if (curr_heuristic < best_heuristic) {
        best_filter = filter_idx;
        best_heuristic = curr_heuristic;
        std::swap(curr_scanline, best_scanline);
      }
    };

    applyFilterAndUpdateBest(1, filter_sub);

    if (y > 0) {
      applyFilterAndUpdateBest(2, filter_up);
      applyFilterAndUpdateBest(3, filter_avg);
      applyFilterAndUpdateBest(4, filter_paeth);
    }

    append<uint8_t>(filtered, best_filter);
    if (best_filter == 0) {
      filtered.append(reinterpret_cast<char const*>(&img(0, y)), img.w_ * 4);
    } else {
      filtered.append(best_scanline.data(), best_scanline.size());
    }
  }

  return compress_deflate(filtered);
}

std::string encode_png(bitmap const& img) {
  std::string buf;

  // write signature
  append<uint8_t>(buf, 137);
  append<uint8_t>(buf, 80);
  append<uint8_t>(buf, 78);
  append<uint8_t>(buf, 71);
  append<uint8_t>(buf, 13);
  append<uint8_t>(buf, 10);
  append<uint8_t>(buf, 26);
  append<uint8_t>(buf, 10);

  {  // write chunk 0
    std::string ihdr;
    append_network_byte_order32(ihdr, img.w_);  // width
    append_network_byte_order32(ihdr, img.h_);  // height
    append<uint8_t>(ihdr, 8);  // bit depth
    append<uint8_t>(ihdr, 6);  // color type RGBA
    append<uint8_t>(ihdr, 0);  // compression method (0 = deflate)
    append<uint8_t>(ihdr, 0);  // filter method (0 = default)
    append<uint8_t>(ihdr, 0);  // interlace method (0 = none);

    append_chunk(buf, "IHDR", std::move(ihdr));
  }

  // write image chunk
  append_chunk(buf, "IDAT", encode_bitmap(img));

  // write end chunk
  append_chunk(buf, "IEND", std::string{});

  return buf;
}

}  // namespace tiles
