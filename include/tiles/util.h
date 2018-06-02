#pragma once

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

namespace tiles {

std::string compress_gzip(std::string const&);

template <typename Fun>
struct raii_helper {
  raii_helper(Fun&& fun) : fun_{std::move(fun)} {}
  ~raii_helper() { fun_(); }

  Fun fun_;
};

}  // namespace tiles
