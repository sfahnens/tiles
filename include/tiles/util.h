#pragma once

#include <time.h>
#include <charconv>
#include <chrono>
#include <atomic>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "utl/verify.h"

namespace tiles {
template <typename... Args>
inline void t_log(Args&&... args) {
  using clock = std::chrono::system_clock;
  auto const now = clock::to_time_t(clock::now());
  struct tm tmp;
#if _MSC_VER >= 1400
  gmtime_s(&tmp, &now);
#else
  gmtime_r(&now, &tmp);
#endif
  std::clog << std::put_time(&tmp, "%FT%TZ") << " | ";
  fmt::print(std::clog, std::forward<Args>(args)...);
  std::clog << std::endl;
}

std::string compress_deflate(std::string const&);

struct progress_tracker {
  explicit progress_tracker(std::string label, size_t total)
      : label_{std::move(label)},
        total_{total},
        curr_{0},
        pos_{std::numeric_limits<size_t>::max()} {}

  void update(size_t new_curr) {
    // see https://stackoverflow.com/a/16190791
    size_t old_curr = curr_;
    while (old_curr < new_curr &&
           !curr_.compare_exchange_weak(old_curr, new_curr))
      ;

    log_progress_maybe();
  }

  void inc(size_t i = 1) {
    curr_ += i;
    log_progress_maybe();
  }

  void log_progress_maybe() {
#ifdef MOTIS_IMPORT_PROGRESS_FORMAT
    size_t curr_pos = static_cast<size_t>(100. * curr_ / total_);
#else
    size_t curr_pos = static_cast<size_t>(100. * curr_ / total_ / 5) * 5;
#endif

    size_t prev_pos = pos_.exchange(curr_pos);
    if (prev_pos != curr_pos) {
      t_log("{} : {:>3}%", label_, curr_pos);

#ifdef MOTIS_IMPORT_PROGRESS_FORMAT
      std::clog << '\0' << curr_pos << '\0' << std::flush;
#endif
    }
  }

  std::string label_;
  size_t total_;
  std::atomic_size_t curr_;
  std::atomic_size_t pos_;
};

template <typename Fun>
struct raii_helper {
  raii_helper(Fun&& fun) : fun_{std::move(fun)} {}
  ~raii_helper() { fun_(); }

  Fun fun_;
};

struct scoped_timer final {
  explicit scoped_timer(std::string label)
      : label_{std::move(label)}, start_{std::chrono::steady_clock::now()} {
    std::clog << "|> start: " << label_ << "\n";
  }

  ~scoped_timer() {
    using namespace std::chrono;

    auto const now = steady_clock::now();
    double dur = duration_cast<microseconds>(now - start_).count() / 1000.0;

    std::clog << "|> done: " << label_ << " (";
    if (dur < 1000) {
      std::clog << std::setw(6) << std::setprecision(4) << dur << "ms";
    } else {
      dur /= 1000;
      std::clog << std::setw(6) << std::setprecision(4) << dur << "s";
    }
    std::clog << ")" << std::endl;
  }

  std::string label_;
  std::chrono::time_point<std::chrono::steady_clock> start_;
};

template <typename Container, typename Fn>
void transform_erase(Container& c, Fn&& fn) {
  if (c.empty()) {
    return;
  }

  auto it = std::begin(c);
  fn(*it);

  for (auto it2 = std::next(it); it2 != std::end(c); ++it2) {
    fn(*it2);
    if (!(*it == *it2)) {
      *++it = std::move(*it2);
    }
  }

  c.erase(++it, std::end(c));
}

struct printable_num {
  explicit printable_num(double n) : n_{n} {}
  explicit printable_num(uint64_t n) : n_{static_cast<double>(n)} {}
  double n_;
};

struct printable_ns {
  explicit printable_ns(double n) : n_{n} {}
  explicit printable_ns(uint64_t n) : n_{static_cast<double>(n)} {}
  double n_;
};

struct printable_bytes {
  explicit printable_bytes(double n) : n_{n} {}
  explicit printable_bytes(uint64_t n) : n_{static_cast<double>(n)} {}
  double n_;
};

}  // namespace tiles

namespace fmt {

template <>
struct formatter<tiles::printable_num> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(tiles::printable_num const& num, FormatContext& ctx) {
    auto const n = num.n_;
    auto const k = n / 1e3;
    auto const m = n / 1e6;
    auto const g = n / 1e9;
    if (n < 1e3) {
      return format_to(ctx.out(), "{:>6} ", n);
    } else if (k < 1e3) {
      return format_to(ctx.out(), "{:>6.1f}K", k);
    } else if (m < 1e3) {
      return format_to(ctx.out(), "{:>6.1f}M", m);
    } else {
      return format_to(ctx.out(), "{:>6.1f}G", g);
    }
  }
};

template <>
struct formatter<tiles::printable_ns> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(tiles::printable_ns const& num, FormatContext& ctx) {
    auto const ns = num.n_;
    auto const mys = ns / 1e3;
    auto const ms = ns / 1e6;
    auto const s = ns / 1e9;
    if (ns < 1e3) {
      return format_to(ctx.out(), "{:>7.3f}ns", ns);
    } else if (mys < 1e3) {
      return format_to(ctx.out(), "{:>7.3f}µs", mys);
    } else if (ms < 1e3) {
      return format_to(ctx.out(), "{:>7.3f}ms", ms);
    } else {
      return format_to(ctx.out(), "{:>7.3f}s ", s);
    }
  }
};

template <>
struct formatter<tiles::printable_bytes> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(tiles::printable_bytes const& bytes, FormatContext& ctx) {
    auto const n = bytes.n_;
    auto const k = n / 1024;
    auto const m = n / (1024 * 1024);
    auto const g = n / (1024 * 1024 * 1024);
    if (n < 1024) {
      return format_to(ctx.out(), "{:>7.2f}B ", n);
    } else if (k < 1024) {
      return format_to(ctx.out(), "{:>7.2f}KB", k);
    } else if (m < 1024) {
      return format_to(ctx.out(), "{:>7.2f}MB", m);
    } else {
      return format_to(ctx.out(), "{:>7.2f}GB", g);
    }
  }
};

}  // namespace fmt

namespace tiles {

inline uint32_t stou(std::string_view sv) {
  uint32_t var;
  auto result = std::from_chars(sv.data(), sv.data() + sv.size(), var);
  utl::verify(result.ec == std::errc(), "cannot convert to uint32_t: {}", sv);
  return var;
}

struct regex_matcher {
  using match_result_t = std::optional<std::vector<std::string_view>>;

  explicit regex_matcher(std::string pattern);
  ~regex_matcher();

  match_result_t match(std::string_view) const;

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace tiles
