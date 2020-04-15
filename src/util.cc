#include "tiles/util.h"

#include <regex>

#include "zlib.h"

#include "utl/verify.h"

namespace tiles {

std::string compress_deflate(std::string const& input) {
  auto out_size = compressBound(input.size());
  std::string buffer(out_size, '\0');

  auto error = compress2(reinterpret_cast<uint8_t*>(&buffer[0]), &out_size,
                         reinterpret_cast<uint8_t const*>(&input[0]),
                         input.size(), Z_BEST_COMPRESSION);
  utl::verify(!error, "compress_deflate failed");

  buffer.resize(out_size);
  return buffer;
}

struct regex_matcher::impl {
  explicit impl(std::regex regex) : regex_{std::move(regex)} {}

  match_result_t match(std::string target) const {
    std::smatch match;
    if (std::regex_match(target, match, regex_)) {
      std::vector<std::string> matches;
      matches.reserve(match.size());
      for (auto i = 0ULL; i < match.size(); ++i) {
        matches.push_back(std::string{match[i].first, match[i].second});
      }
      return matches;
    }
    return std::nullopt;
  }

  std::regex regex_;
};

regex_matcher::regex_matcher(std::string pattern)
    : impl_{std::make_unique<regex_matcher::impl>(std::regex{pattern})} {}

regex_matcher::~regex_matcher() = default;

regex_matcher::match_result_t regex_matcher::match(std::string target) const {
  return impl_->match(target);
}

}  // namespace tiles
