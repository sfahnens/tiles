#include "tiles/osm/load_shapefile.h"

#include <iostream>

#include "miniz.c"

#include "utl/parser/buffer.h"
#include "utl/parser/file.h"
#include "utl/parser/mmap_reader.h"

#include "tiles/fixed/convert.h"
#include "tiles/fixed/algo/area.h"
#include "tiles/fixed/fixed_geometry.h"
#include "tiles/util.h"

namespace tiles {

int32_t read_int_big(utl::buffer const& buf, size_t const pos) {
  return (buf.data()[pos + 3] << 0) |  //
         (buf.data()[pos + 2] << 8) |  //
         (buf.data()[pos + 1] << 16) |  //
         (buf.data()[pos + 0] << 24);
}

int32_t read_int_little(utl::buffer const& buf, size_t const pos) {
  int32_t val;
  std::memcpy(&val, buf.data() + pos, sizeof(int32_t));
  return val;
}

double read_double_little(utl::buffer const& buf, size_t const pos) {
  double val;
  std::memcpy(&val, buf.data() + pos, sizeof(double));
  return val;
}

template <typename Vec>
auto emplace_back_ref(Vec& vec) -> decltype(vec.back()) {
  vec.emplace_back();
  return vec.back();
}

std::vector<fixed_geometry> read_shapefile(utl::buffer const& buf) {
  verify(9994 == read_int_big(buf, 0), "shp: invalid magic number");
  verify(1000 == read_int_little(buf, 28), "shp: invalid file version");
  verify(5 == read_int_little(buf, 32), "shp: only polygons supported (main)");

  std::vector<fixed_geometry> result;

  int index = 0;
  size_t rh_offset = 100;
  while (rh_offset < buf.size()) {
    verify(++index == read_int_big(buf, rh_offset), "shp: unexpected index");

    fixed_polygon polygon;
    auto& simple_polygon = emplace_back_ref(polygon);
    auto const read_ring = [&buf, &simple_polygon](
        auto const pts_offset, auto const idx_lb, auto const idx_ub) {
      auto& ring = simple_polygon.outer().empty()
                       ? simple_polygon.outer()
                       : emplace_back_ref(simple_polygon.inners());

      auto const count = idx_ub - idx_lb;
      ring.reserve(count);
      for (auto i = 0; i < count; ++i) {
        auto const pt_offset = pts_offset + 16 * i;

        auto lng = read_double_little(buf, pt_offset);
        auto lat = read_double_little(buf, pt_offset + 8);

        ring.emplace_back(latlng_to_fixed({lat, lng}));
      }
    };

    auto const rc_offset = rh_offset + 8;

    auto const lng_min = read_double_little(buf, rc_offset + 4);
    auto const lat_min = read_double_little(buf, rc_offset + 12);
    auto const lng_max = read_double_little(buf, rc_offset + 20);
    auto const lat_max = read_double_little(buf, rc_offset + 28);

    // std::cout << "(" << lat_min << ", " << lng_min << ") - (" << lat_max <<
    // ", "
    //           << lng_max << ")\n";

    // if (lat_min < 64.1 && lat_max > 64.1 &&  //
    //     lng_min < -21.5 && lng_max > -21.5) {

    verify(5 == read_int_little(buf, rc_offset),
           "shp: only polygons supported");

    auto const num_parts = read_int_little(buf, rc_offset + 36);
    auto const num_points = read_int_little(buf, rc_offset + 40);

    verify(num_parts > 0, "shp: need at least one part");
    verify(num_points > 0, "shp: need at least one point");

    if (num_points > 1e6) {

      auto const parts_offset = rc_offset + 44;
      auto const pts_offset = parts_offset + 4 * num_parts;
      for (auto i = 0; i < num_parts - 1; ++i) {
        read_ring(pts_offset,  //
                  read_int_little(buf, parts_offset + i * 4),
                  read_int_little(buf, parts_offset + i * 4 + 4));
      }
      read_ring(pts_offset,  //
                read_int_little(buf, parts_offset + 4 * (num_parts - 1)),  //
                num_points);


      boost::geometry::correct(polygon);

      std::cout << "area: " << tiles::area(polygon) << std::endl;

      verify(!simple_polygon.outer().empty(), "shp: read polygon is empty?!");
      result.emplace_back(polygon);
    }

    rh_offset += 8 + read_int_big(buf, rh_offset + 4) * 2;
    verify(rh_offset <= buf.size(), "shp: offset limit violation");
  }

  return result;
}

utl::buffer load_buffer(std::string const& fname) {
  utl::mmap_reader mem{fname.c_str()};

  mz_zip_archive ar{};
  verify(mz_zip_reader_init_mem(&ar, mem.m_.ptr(), mem.m_.size(), 0),
         "shp: invalid zip");
  raii_helper ar_deleter{[&ar] { mz_zip_reader_end(&ar); }};

  auto n = mz_zip_reader_get_num_files(&ar);
  for (auto i = 0u; i < n; ++i) {
    mz_zip_archive_file_stat stat{};
    verify(mz_zip_reader_file_stat(&ar, i, &stat),
           "shp: unable to stat zip entry");

    std::string_view name{stat.m_filename};
    if (name.size() < 4 || name.substr(name.size() - 4) != ".shp") {
      continue;
    }

    std::cout << stat.m_filename << std::endl;

    utl::buffer buf{stat.m_uncomp_size};
    verify(mz_zip_reader_extract_to_mem(&ar, i, buf.data(), buf.size(), 0),
           "shp: error extracting .shp file");
    return buf;
  }
  verify(false, "shp: .zip file contains no .shp file")
}

std::vector<fixed_geometry> load_shapefile(std::string const& fname) {
  std::cout << "load buffer" << std::endl;
  auto&& buf = load_buffer(fname);
  std::cout << "read_shapefile" << std::endl;

  auto&& geo = read_shapefile(buf);
  std::cout << "done." << std::endl;

  return geo;
}

}  // namespace tiles
