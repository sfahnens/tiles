#include <exception>
#include <iomanip>
#include <iostream>

#include "boost/filesystem.hpp"

#include "conf/options_parser.h"
#include "conf/simple_config.h"

#include "net/http/server/enable_cors.hpp"
#include "net/http/server/query_router.hpp"
#include "net/http/server/server.hpp"
#include "net/http/server/shutdown_handler.hpp"
#include "net/http/server/url_decode.hpp"

#include "utl/parser/mmap_reader.h"

#include "tiles/db/get_tile.h"
#include "tiles/db/tile_database.h"
#include "tiles/perf_counter.h"

#include "tiles_server_res.h"

using namespace net::http::server;

struct server_settings : public conf::simple_config {
  explicit server_settings(std::string const& db_fname = "tiles.mdb",
                           std::string const& res_dname = "")
      : simple_config("tiles-server options", "") {
    string_param(db_fname_, db_fname, "db_fname", "/path/to/tiles.mdb");
    string_param(res_dname_, res_dname, "res_dname", "/path/to/res");
  }

  std::string db_fname_;
  std::string res_dname_;
};

int main(int argc, char** argv) {
  server_settings opt;

  try {
    conf::options_parser parser({&opt});
    parser.read_command_line_args(argc, argv, false);

    if (parser.help() || parser.version()) {
      std::cout << "tiles-server\n\n";
      parser.print_help(std::cout);
      return 0;
    }

    parser.read_configuration_file(false);
    parser.print_used(std::cout);
  } catch (std::exception const& e) {
    std::cout << "options error: " << e.what() << "\n";
    return 1;
  }

  lmdb::env db_env = tiles::make_tile_database(opt.db_fname_.c_str());
  tiles::tile_db_handle handle{db_env};
  auto const render_ctx = make_render_ctx(handle);

  boost::asio::io_service ios;
  server server{ios};

  query_router router;
  router.route("OPTIONS", ".*", [](auto const&, auto cb) {
    reply rep = reply::stock_reply(reply::ok);
    add_cors_headers(rep);
    cb(rep);
  });

  // z, x, y
  router.route(
      "GET", "^\\/(\\d+)\\/(\\d+)\\/(\\d+).mvt$",
      [&](auto const& req, auto cb) {
        if (std::find_if(begin(req.headers), end(req.headers),
                         [](auto const& h) {
                           return h.name == "Accept-Encoding" &&
                                  h.value.find("deflate") != std::string::npos;
                         }) == end(req.headers)) {
          return cb(reply::stock_reply(reply::not_implemented));
        }

        try {
          tiles::t_log("received a request: {}", req.uri);
          auto const tile =
              geo::tile{static_cast<uint32_t>(std::stoul(req.path_params[1])),
                        static_cast<uint32_t>(std::stoul(req.path_params[2])),
                        static_cast<uint32_t>(std::stoul(req.path_params[0]))};

          tiles::perf_counter pc;
          auto rendered_tile = tiles::get_tile(handle, render_ctx, tile, pc);
          tiles::perf_report_get_tile(pc);

          reply rep = reply::stock_reply(reply::ok);
          if (rendered_tile) {
            rep.headers.emplace_back("Content-Encoding", "deflate");
            rep.content = std::move(*rendered_tile);
          } else {
            rep.status = reply::no_content;
          }

          add_cors_headers(rep);
          return cb(rep);
        } catch (std::exception const& e) {
          tiles::t_log("unhandled error: {}", e.what());
        } catch (...) {
          tiles::t_log("unhandled unknown error");
        }
      });

  auto const serve_file = [&](auto const& fname, auto cb) {
    try {
      std::string decoded_fname;
      if (!url_decode(fname, decoded_fname)) {
        return cb(reply::stock_reply(reply::bad_request));
      }

      reply rep;
      rep.status = reply::status_type::ok;
      rep.headers = {{"Content-Type", ""}};  // stupid hack
      add_cors_headers(rep);

      if (opt.res_dname_.size() != 0) {
        auto p = boost::filesystem::path{opt.res_dname_} / decoded_fname;
        if (boost::filesystem::exists(p)) {
          utl::mmap_reader mem{p.c_str()};
          rep.content = std::string{mem.m_.ptr(), mem.m_.size()};
          return cb(rep);
        }
      }

      auto const mem = tiles_server_res::get_resource(decoded_fname);
      rep.content =
          std::string{reinterpret_cast<char const*>(mem.ptr_), mem.size_};
      return cb(rep);
    } catch (std::exception const& e) {
      return cb(reply::stock_reply(reply::not_found));
    } catch (...) {
      return cb(reply::stock_reply(reply::internal_server_error));
    }
  };

  router.route("GET", "^\\/(.+)$", [&](auto const& req, auto cb) {
    return serve_file(req.path_params[0], cb);
  });

  router.route("GET", "^\\/$", [&](auto const&, auto cb) {
    return serve_file("index.html", cb);
  });

  server.listen("0.0.0.0", "8888", router);

  io_service_shutdown shutd(ios);
  shutdown_handler<io_service_shutdown> shutdown(ios, shutd);

  tiles::t_log(">>> tiles-server up and running!");
  while (true) {
    try {
      ios.run();
      break;
    } catch (std::exception const& e) {
      tiles::t_log("unhandled error: {}", e.what());
    } catch (...) {
      tiles::t_log("unhandled unknown error");
    }
  }
}
