#pragma once

namespace tiles {

static_assert(std::numeric_limits<double>::is_iec559, "IEEE-754 required");

constexpr uint64_t kIsSpecial = 0xFFFull << 52;  // nan prefix

constexpr uint64_t kTypeMask = 0b11;
constexpr uint64_t kTypeShift = 50;

constexpr uint64_t kCountMask = 0x4'FFFF'FFFF'FFFF;

enum feature_type { POINT = 1, POLYLINE = 2, POLYGON = 3 };

union flat_geometry {

  flat_geometry() = default;

  flat_geometry(feature_type const type, uint64_t const count = 1) {
    special_ = kIsSpecial |  //
               (static_cast<uint64_t>(type) << kTypeShift) |
               (count & kCountMask);
  }

  explicit flat_geometry(double const val) : val_(val) {}

  uint64_t special_;
  double val_;
};

template <typename Slice>
size_t get_size(Slice const& slice) {
  return slice.size() / sizeof(flat_geometry);
}

template <typename Slice>
flat_geometry get_elem(Slice const& slice, size_t const idx) {
  return reinterpret_cast<flat_geometry const*>(slice.data())[idx];
}

template <typename Slice>
feature_type get_type(Slice const& slice) {
  auto const type_int =
      (get_elem(slice, 0ul).special_ & (kTypeMask << kTypeShift)) >> kTypeShift;
  return static_cast<feature_type>(type_int);
}

template <typename Slice>
double get_value(Slice const& slice, size_t const idx) {
  return get_elem(slice, idx).val_;
}

template <typename Slice>
bool is_special(Slice const& slice, size_t const idx) {
  return (get_elem(slice, idx).special_ & kIsSpecial) == kIsSpecial;
}

template <typename Slice>
size_t get_count(Slice const& slice, size_t const idx) {
  return get_elem(slice, idx).special_ & kCountMask;
}

}  // namespace tiles
