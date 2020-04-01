#include "tiles/mvt/tile_builder.h"

#include <iostream>
#include <limits>
#include <unordered_set>

#include "boost/algorithm/string/predicate.hpp"

#include "utl/get_or_create.h"
#include "utl/get_or_create_index.h"

#include "tiles/fixed/algo/clip.h"
#include "tiles/fixed/algo/shift.h"
#include "tiles/fixed/io/deserialize.h"
#include "tiles/fixed/io/dump.h"

#include "tiles/bin_utils.h"
#include "tiles/get_tile.h"
#include "tiles/mvt/encode_geometry.h"
#include "tiles/mvt/tags.h"
#include "tiles/util.h"

using namespace protozero;
namespace ttm = tiles::tags::mvt;

namespace tiles {

struct layer_builder {
  layer_builder(render_ctx const& ctx, std::string const& name,
                tile_spec const& spec)
      : ctx_{ctx}, spec_(spec), has_geometry_(false), buf_(), pb_(buf_) {
    pb_.add_uint32(ttm::Layer::required_uint32_version, 2);
    pb_.add_string(ttm::Layer::required_string_name, name);
    pb_.add_uint32(ttm::Layer::optional_uint32_extent, 4096);
  }

  void add_feature(feature const& f) {
    std::string feature_buf;
    pbf_builder<ttm::Feature> feature_pb(feature_buf);

    if ((mpark::holds_alternative<fixed_point>(f.geometry_) &&
         !node_ids_.insert(f.id_).second) ||
        (mpark::holds_alternative<fixed_polyline>(f.geometry_) &&
         !line_ids_.insert(f.id_).second) ||
        (mpark::holds_alternative<fixed_polygon>(f.geometry_) &&
         !poly_ids_.insert(f.id_).second)) {
      return;
    }

    if (write_geometry(feature_pb, f)) {
      has_geometry_ = true;

      feature_pb.add_uint64(ttm::Feature::optional_uint64_id, f.id_);
      write_metadata(feature_pb, f.meta_);
      pb_.add_message(ttm::Layer::repeated_Feature_features, feature_buf);
    }
  }

  bool write_geometry(pbf_builder<ttm::Feature>& pb, feature const& f) {
    auto geometry = clip(f.geometry_, spec_.draw_bounds_);

    if (mpark::holds_alternative<fixed_null>(geometry)) {
      return false;
    }

    shift(geometry, spec_.tile_.z_);
    encode_geometry(pb, geometry, spec_);
    return true;
  }

  void write_metadata(pbf_builder<ttm::Feature>& pb,
                      std::vector<metadata> const& meta) {
    std::vector<uint32_t> t;

    for (auto const& m : meta) {
      if (m.key_ == "layer" || boost::starts_with(m.key_, "__")) {
        continue;
      }

      t.emplace_back(utl::get_or_create_index(meta_key_cache_, m.key_));
      t.emplace_back(utl::get_or_create_index(meta_value_cache_, m.value_));
    }

    pb.add_packed_uint32(ttm::Feature::packed_uint32_tags, begin(t), end(t));
  }

  void render_debug_info() {
    auto const& min = spec_.px_bounds_.min_corner();
    auto const& max = spec_.px_bounds_.max_corner();
    fixed_geometry line = fixed_polyline{{{min.x(), min.y()},
                                          {min.x(), max.y()},
                                          {max.x(), max.y()},
                                          {max.x(), min.y()},
                                          {min.x(), min.y()}}};
    {
      std::string feature_buf;
      pbf_builder<ttm::Feature> feature_pb(feature_buf);

      encode_geometry(feature_pb, line, spec_);
      pb_.add_message(ttm::Layer::repeated_Feature_features, feature_buf);
    }
  }

  std::string finish() {
    std::vector<std::string const*> keys(meta_key_cache_.size());
    for (auto const& pair : meta_key_cache_) {
      keys[pair.second] = &pair.first;
    }
    for (auto const& key : keys) {
      pb_.add_string(ttm::Layer::repeated_string_keys, *key);
    }

    std::vector<std::string const*> values(meta_value_cache_.size());
    for (auto const& pair : meta_value_cache_) {
      values[pair.second] = &pair.first;
    }
    for (auto const& value : values) {
      pbf_builder<ttm::Value> val_pb(pb_, ttm::Layer::repeated_Value_values);

      static_assert(sizeof(metadata_value_t) == 1);
      utl::verify(!value->empty(), "tile_builder: have empty value");
      switch (read<metadata_value_t>(value->data())) {
        case metadata_value_t::bool_false:
          val_pb.add_bool(ttm::Value::optional_bool_bool_value, false);
          break;
        case metadata_value_t::bool_true:
          val_pb.add_bool(ttm::Value::optional_bool_bool_value, true);
          break;
        case metadata_value_t::string:
          val_pb.add_string(ttm::Value::optional_string_string_value,
                            value->data() + 1, value->size() - 1);
          break;
        case metadata_value_t::numeric:
          utl::verify(value->size() == 1 + sizeof(double),
                      "tile_builder: invalid numeric feature");
          val_pb.add_double(ttm::Value::optional_double_double_value,
                            read<double>(value->data(), 1));
          break;
        case metadata_value_t::integer:
          utl::verify(value->size() == 1 + sizeof(int64_t),
                      "tile_builder: invalid integer feature");
          val_pb.add_sint64(ttm::Value::optional_sint64_sint_value,
                            read<int64_t>(value->data(), 1));
          break;
        default: throw utl::fail("tile_builder: unknown metadata_value_t");
      }
    }

    return buf_;
  }

  render_ctx const& ctx_;
  tile_spec const& spec_;

  bool has_geometry_;

  std::string buf_;
  pbf_builder<ttm::Layer> pb_;

  std::map<std::string, size_t> meta_key_cache_;
  std::map<std::string, size_t> meta_value_cache_;

  std::unordered_set<uint64_t> node_ids_, line_ids_, poly_ids_;
};

struct tile_builder::impl {
  impl(render_ctx const& ctx, geo::tile const& tile) : ctx_{ctx}, spec_{tile} {}

  void add_feature(feature const& f) {
    utl::verify(f.layer_ < ctx_.layer_names_.size(), "invalid layer in db");

    utl::get_or_create(builders_, f.layer_, [&] {
      return std::make_unique<layer_builder>(
          ctx_, ctx_.layer_names_.at(f.layer_), spec_);
    })->add_feature(f);
  }

  std::string finish() {
    std::string buf;
    pbf_builder<ttm::Tile> pb(buf);

    for (auto const& pair : builders_) {
      if (!pair.second->has_geometry_) {
        continue;
      }

      if (ctx_.tb_render_debug_info_) {
        pair.second->render_debug_info();
      }

      pb.add_message(ttm::Tile::repeated_Layer_layers, pair.second->finish());
    }

    return buf;
  }

  render_ctx const& ctx_;
  tile_spec spec_;
  std::map<size_t, std::unique_ptr<layer_builder>> builders_;
};

tile_builder::tile_builder(render_ctx const& ctx, geo::tile const& tile)
    : impl_(std::make_unique<impl>(ctx, tile)) {}

tile_builder::~tile_builder() = default;

void tile_builder::add_feature(feature const& f) { impl_->add_feature(f); }

std::string tile_builder::finish() { return impl_->finish(); }

}  // namespace tiles
