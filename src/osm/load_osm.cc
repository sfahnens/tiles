#include "tiles/osm/load_osm.h"

#include "osmium/area/assembler.hpp"
#include "osmium/area/multipolygon_manager.hpp"
#include "osmium/io/pbf_input.hpp"
#include "osmium/memory/buffer.hpp"
#include "osmium/util/progress_bar.hpp"
#include "osmium/visitor.hpp"

#include "tiles/db/tile_database.h"
#include "tiles/osm/feature_handler.h"
#include "tiles/osm/hybrid_node_idx.h"
#include "tiles/util_parallel.h"

namespace tiles {

namespace o = osmium;
namespace oa = osmium::area;
namespace oio = osmium::io;
namespace oh = osmium::handler;
namespace orel = osmium::relations;
namespace om = osmium::memory;
namespace ou = osmium::util;
namespace oeb = osmium::osm_entity_bits;

void load_osm(tile_db_handle& handle, std::string const& fname) {
  oio::File input_file{fname};

  oa::MultipolygonManager<oa::Assembler> mp_manager{
      oa::Assembler::config_type{}};

  hybrid_node_idx node_idx{fileno(std::fopen("idx.bin", "wb+")),
                           fileno(std::fopen("dat.bin", "wb+"))};

  {
    t_log("Pass 1...");
    hybrid_node_idx_builder node_idx_builder{node_idx};

    o::ProgressBar progress_bar{input_file.size(), ou::isatty(2)};
    oio::Reader reader{input_file, oeb::node | oeb::relation};
    while (auto buffer = reader.read()) {
      progress_bar.update(reader.offset());
      o::apply(buffer, node_idx_builder, mp_manager);
    }
    reader.close();
    progress_bar.file_done(input_file.size());
    t_log("Pass 1 done");

    mp_manager.prepare_for_lookup();
    t_log("Multipolygon Manager Memory:");
    orel::print_used_memory(std::cout, mp_manager.used_memory());

    node_idx_builder.finish();
    t_log("Hybrid Node Index Statistics:");
    node_idx_builder.dump_stats();
  }

  feature_inserter_mt inserter{
      dbi_handle{handle, handle.features_dbi_opener()}};
  layer_names_builder names_builder;

  in_order_queue<om::Buffer> mp_queue;

  {
    t_log("Pass 2...");
    auto const thread_count =
        static_cast<int>(std::thread::hardware_concurrency());

    // poor mans thread local (we dont know the threads themselves)
    std::atomic_size_t next_handlers_slot{0};
    std::vector<std::pair<std::thread::id, feature_handler>> handlers;
    for (auto i = 0; i < thread_count; ++i) {
      handlers.emplace_back(std::thread::id{},
                            feature_handler{inserter, names_builder});
    }
    auto const get_handler = [&]() -> feature_handler& {
      auto const thread_id = std::this_thread::get_id();
      if (auto it = std::find_if(
              begin(handlers), end(handlers),
              [&](auto const& pair) { return pair.first == thread_id; });
          it != end(handlers)) {
        return it->second;
      }
      auto slot = next_handlers_slot.fetch_add(1);
      utl::verify(slot < handlers.size(), "more threads than expected");
      handlers[slot].first = thread_id;
      return handlers[slot].second;
    };

    // pool must be destructed before handlers!
    osmium::thread::Pool pool{thread_count,
                              static_cast<size_t>(thread_count * 8)};

    // use reader with progress bar or something
    oio::Reader reader{input_file, pool};
    progress_tracker reader_progress{"load osm features", reader.file_size()};
    sequential_until_finish<om::Buffer> seq_reader{[&] {
      reader_progress.update(reader.offset());
      return reader.read();
    }};

    // TODO check pool size (leave at least one free for)
    std::vector<std::future<void>> workers;
    for (auto i = 0; i < thread_count / 2; ++i) {
      workers.emplace_back(pool.submit([&] {
        try {
          while (true) {
            auto opt = seq_reader.process();
            if (!opt.has_value()) {
              break;
            }

            auto& [idx, buf] = *opt;
            update_locations(node_idx, buf);
            o::apply(buf, get_handler());

            mp_queue.process_in_order(idx, std::move(buf), [&](auto buf2) {
              o::apply(buf2, mp_manager.handler([&](auto&& mp_buffer) {
                pool.submit([mp_buffer = std::move(mp_buffer), &get_handler] {
                  o::apply(mp_buffer, get_handler());
                });
              }));
            });
          }
        } catch (std::exception const& e) {
          std::cout << "EX " << std::this_thread::get_id() << " " << e.what()
                    << "\n";
        } catch (...) {
          std::cout << "EX " << std::this_thread::get_id() << "\n";
        }
      }));
    }
    utl::verify(!workers.empty(), "have no workers");
    for (auto& worker : workers) {
      worker.wait();
    }

    utl::verify(mp_queue.queue_.empty(), "mp_queue not empty!");

    reader.close();
    // progress_bar.file_done(input_file.size());
    t_log("Pass 2 done");

    t_log("Multipolygon Manager Memory:");
    orel::print_used_memory(std::cout, mp_manager.used_memory());
  }

  {
    lmdb::txn txn = handle.make_txn();
    names_builder.store(handle, txn);
    txn.commit();
  }
}

}  // namespace tiles
