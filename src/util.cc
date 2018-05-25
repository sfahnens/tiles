#include "tiles/util.h"

#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/filtering_stream.hpp"

namespace tiles {

std::string compress_gzip(std::string const& input) {
  namespace bio = boost::iostreams;

  std::string output;
  bio::filtering_ostream os;

  os.push(bio::gzip_compressor(bio::gzip_params(bio::gzip::best_compression)));
  os.push(bio::back_inserter(output));

  os << input;
  bio::close(os);

  return output;
}
}  // namespace tiles
