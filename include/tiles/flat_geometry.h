#pragma once

namespace tiles {

constexpr auto kPointFeature = 1.0;
constexpr auto kPolylineFeature = 2.0;
constexpr auto kPolygonFeature = 3.0;

template <typename Slice>
double get_elem(Slice const& slice, size_t const idx) {
  return reinterpret_cast<double const*>(slice.data())[idx];
}

template <typename Slice>
double get_type(Slice const& slice) {
  return get_elem(slice, 0);
}

template <typename Slice>
size_t get_count(Slice const& slice) {
  return slice.size() / sizeof(double);
}

}  // namespace tiles
