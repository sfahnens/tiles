#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#ifndef log_err
#define log_err(M, ...) fprintf(stderr, "[ERR] " M "\n", ##__VA_ARGS__);
#endif

#ifdef verify
#undef verify
#endif

#define verify(A, M, ...)        \
  if (!(A)) {                    \
    log_err(M, ##__VA_ARGS__);   \
    throw std::runtime_error(M); \
  }

#define verify_silent(A, M, ...) \
  if (!(A)) {                    \
    throw std::runtime_error(M); \
  }

namespace tiles {

std::string compress_gzip(std::string const&);

template <typename Fun>
struct raii_helper {
  raii_helper(Fun&& fun) : fun_{std::move(fun)} {}
  ~raii_helper() { fun_(); }

  Fun fun_;
};

struct scoped_timer final {
  explicit scoped_timer(std::string label)
      : label_{std::move(label)}, start_{std::chrono::steady_clock::now()} {
    std::cout << "|> start: " << label_ << "\n";
  }

  ~scoped_timer() {
    using namespace std::chrono;

    auto const now = steady_clock::now();
    double dur = duration_cast<microseconds>(now - start_).count() / 1000.0;

    std::cout << "|> done: " << label_ << " (";
    if (dur < 1000) {
      std::cout << std::setw(6) << std::setprecision(4) << dur << "ms";
    } else {
      dur /= 1000;
      std::cout << std::setw(6) << std::setprecision(4) << dur << "s";
    }
    std::cout << ")" << std::endl;
  }

  std::string label_;
  std::chrono::time_point<std::chrono::steady_clock> start_;
};

}  // namespace tiles
