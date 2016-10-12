#include "tiles/mvt/builder.h"

#include <iostream>
#include <limits>

#include "tiles/mvt/encoder.h"
#include "tiles/mvt/tags.h"
#include "tiles/util.h"

using namespace rocksdb;
using namespace rocksdb::spatial;

using namespace protozero;

namespace tiles {

struct variant_less {
  bool operator()(Variant const& a, Variant const& b) {
    if (a.type() != b.type()) {
      return a.type() < b.type();
    }

    switch (a.type()) {
      case Variant::Type::kBool: return a.get_bool() < b.get_bool();
      case Variant::Type::kInt: return a.get_int() < b.get_int();
      case Variant::Type::kDouble: return a.get_double() < b.get_double();
      case Variant::Type::kString: return a.get_string() < b.get_string();
      default: return false;
    }
  }
};

struct layer_builder {
  layer_builder(std::string const& name, tile_spec const& spec)
      : spec_(spec), buf_(), pb_(buf_) {
    pb_.add_uint32(tags::Layer::required_uint32_version, 2);
    pb_.add_string(tags::Layer::required_string_name, name);
    pb_.add_uint32(tags::Layer::optional_uint32_extent, 4096);
  }

  void add_feature(FeatureSet const& meta, Slice const& geo) {
    std::string feature_buf;
    pbf_builder<tags::Feature> feature_pb(feature_buf);

    write_metadata(feature_pb, meta);
    write_geometry(feature_pb, geo);

    pb_.add_message(tags::Layer::repeated_Feature_features, feature_buf);
  }

  void write_metadata(pbf_builder<tags::Feature>& pb, FeatureSet const& meta) {
    std::vector<uint32_t> t;

    for (auto const& pair : meta) {
      if (pair.first == "layer") {
        continue;
      }

      t.emplace_back(get_or_create_index(meta_key_cache_, pair.first));
      t.emplace_back(get_or_create_index(meta_value_cache_, pair.second));
    }

    pb.add_packed_uint32(tags::Feature::packed_uint32_tags, begin(t), end(t));
  }

  void write_geometry(pbf_builder<tags::Feature>& pb, Slice const& geo) {
    auto const encoded = encode_geometry(geo, spec_);
    pb.add_enum(tags::Feature::optional_GeomType_type, encoded.first);
    pb.add_packed_uint32(tags::Feature::packed_uint32_geometry,
                         begin(encoded.second), end(encoded.second));
  }

  std::string finish() {
    std::vector<std::string const*> keys(meta_key_cache_.size());
    for (auto const& pair : meta_key_cache_) {
      keys[pair.second] = &pair.first;
    }
    for (auto const& key : keys) {
      pb_.add_string(tags::Layer::repeated_string_keys, *key);
    }

    std::vector<Variant const*> values(meta_value_cache_.size());
    for (auto const& pair : meta_value_cache_) {
      values[pair.second] = &pair.first;
    }
    for (auto const& value : values) {
      pbf_builder<tags::Value> val_pb(pb_, tags::Layer::repeated_Value_values);

      switch (value->type()) {
        case Variant::Type::kBool:
          val_pb.add_bool(tags::Value::optional_bool_bool_value,
                          value->get_bool());
          break;
        case Variant::Type::kInt:
          val_pb.add_uint64(tags::Value::optional_uint64_uint_value,
                            value->get_int());
          break;
        case Variant::Type::kDouble:
          val_pb.add_double(tags::Value::optional_double_double_value,
                            value->get_double());
          break;
        case Variant::Type::kString:
          val_pb.add_string(tags::Value::optional_string_string_value,
                            value->get_string());
          break;
        // should not happen
        case Variant::Type::kNull:
        default:
          val_pb.add_double(tags::Value::optional_double_double_value,
                            std::numeric_limits<double>::quiet_NaN());
          break;
      }
    }

    return buf_;
  }

  tile_spec const& spec_;

  std::string buf_;
  pbf_builder<tags::Layer> pb_;

  std::map<std::string, size_t> meta_key_cache_;
  std::map<Variant, size_t, variant_less> meta_value_cache_;
};

struct tile_builder::impl {
  explicit impl(tile_spec const& spec) : spec_(spec) {}

  void add_feature(FeatureSet const& meta, Slice const& geo) {
    auto it = meta.Find("layer");
    if (it == meta.end() || (*it).second.type() != Variant::kString) {
      std::cout << "skip invalid feature "
                << (it == meta.end() ? "true" : "false") << " "
                << (*it).second.type() << std::endl;
      return;  // invalid feature
    }

    get_or_create(builders_, (*it).second.get_string(), [&]() {
      return std::make_unique<layer_builder>((*it).second.get_string(), spec_);
    })->add_feature(meta, geo);
  }

  std::string finish() {
    std::string buf;
    pbf_builder<tags::Tile> pb(buf);

    for (auto const& pair : builders_) {
      std::cout << "append layer: " << pair.first << std::endl;
      pb.add_message(tags::Tile::repeated_Layer_layers, pair.second->finish());
    }

    return buf;
  }

  tile_spec const& spec_;
  std::map<std::string, std::unique_ptr<layer_builder>> builders_;
};

tile_builder::tile_builder(tile_spec const& spec)
    : impl_(std::make_unique<impl>(spec)) {}

tile_builder::~tile_builder() = default;

void tile_builder::add_feature(FeatureSet const& meta, Slice const& geo) {
  impl_->add_feature(meta, geo);
}

std::string tile_builder::finish() { return impl_->finish(); }

}  // namespace tiles
