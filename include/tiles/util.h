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

}  // namespace tiles
