#pragma once

#include <cstring>
#include <vector>

#include "osmium/index/detail/mmap_vector_file.hpp"

#include "tiles/bin_utils.h"

#include "utl/verify.h"

namespace tiles {

struct pack_record {
  friend bool operator==(pack_record const& lhs, pack_record const& rhs) {
    return std::tie(lhs.offset_, lhs.size_) == std::tie(rhs.offset_, rhs.size_);
  }

  friend bool operator<(pack_record const& lhs, pack_record const& rhs) {
    return std::tie(lhs.offset_, lhs.size_) < std::tie(rhs.offset_, rhs.size_);
  }

  size_t offset_, size_;
};

inline std::string pack_records_serialize(pack_record record) {
  std::string buf;
  append(buf, record);
  return buf;
}

inline std::string pack_records_serialize(
    std::vector<pack_record> const& records) {
  std::string buf;
  buf.reserve(records.size() * sizeof(pack_record));
  for (auto const& record : records) {
    append(buf, record);
  }
  return buf;
}

inline void pack_records_update(std::string& dat, pack_record record) {
  utl::verify(dat.size() % sizeof(pack_record) == 0,
              "pack_records_update: invalid pack_record count");
  append(dat, record);
}

inline std::vector<pack_record> pack_records_deserialize(std::string_view dat) {
  utl::verify(dat.size() % sizeof(pack_record) == 0,
              "pack_records_deserialize: invalid pack_record count");
  auto const size = dat.size() / sizeof(pack_record);

  std::vector<pack_record> vec;
  vec.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    vec.push_back(read_nth<pack_record>(dat.data(), i));
  }
  return vec;
}

template <typename Fn>
inline void pack_records_foreach(std::string_view dat, Fn&& fn) {
  utl::verify(dat.size() % sizeof(pack_record) == 0,
              "pack_records_foreach: invalid pack_record count");
  for (size_t i = 0; i < dat.size() / sizeof(pack_record); ++i) {
    fn(read_nth<pack_record>(dat.data(), i));
  }
}

inline std::string pack_file_name(char const* db_fname) {
  std::string fname{db_fname};
  if (fname.size() >= 4 && fname.substr(fname.size() - 4) == ".mdb") {
    fname.replace(fname.size() - 4, 4, ".pck");
  } else {
    fname.append(".pck");
  }
  return fname;
}

inline void clear_pack_file(char const* db_fname) {
  auto const fname = pack_file_name(db_fname);
  auto f = std::fopen(fname.c_str(), "wb+");
  utl::verify(f, "clear_pack_file: failed to fopen {}", fname);
  utl::verify(std::fclose(f) == 0, "clear_pack_file: problem while fclose");
}

struct pack_handle {
  explicit pack_handle(char const* db_fname) {
    auto const fname = pack_file_name(db_fname);
    file_ = std::fopen(fname.c_str(), "rb+");
    if (!file_) {
      file_ = std::fopen(fname.c_str(), "wb+");
    }
    utl::verify(file_, "pack_handle: failed to fopen {}", fname);
    dat_ = osmium::detail::mmap_vector_file<char>{fileno(file_)};
  }

  ~pack_handle() {
    // suffix must not be '\0' (= default value for char)
    if (dat_.empty() || dat_.at(dat_.size() - 1) == '\0') {
      dat_.push_back('a');  // anything else
    }
    utl::verify(std::fclose(file_) == 0, "pack_handle: problem while fclose");
  }

  size_t size() const { return dat_.size(); }

  size_t capacity() const { return dat_.capacity(); }

  void resize(size_t new_size) { dat_.resize(new_size); }

  std::string get(pack_record record) const;

  pack_record insert(size_t offset, std::string_view dat);

  pack_record append(std::string_view dat) { return insert(dat_.size(), dat); }

  pack_record move(size_t offset, pack_record from_record) {
    pack_record to_record{offset, from_record.size_};
    dat_.resize(std::max(dat_.size(), to_record.offset_ + to_record.size_));
    std::memmove(dat_.data() + to_record.offset_,
                 dat_.data() + from_record.offset_, to_record.size_);
    return to_record;
  }

  FILE* file_;
  osmium::detail::mmap_vector_file<char> dat_;
};

}  // namespace tiles
