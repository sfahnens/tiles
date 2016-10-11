#include <exception>
#include <iomanip>
#include <iostream>

#include "boost/filesystem.hpp"

#include "rocksdb/utilities/spatial_db.h"

#include "net/http/server/enable_cors.hpp"
#include "net/http/server/query_router.hpp"
#include "net/http/server/server.hpp"
#include "net/http/server/shutdown_handler.hpp"

#include "geo/webmercator.h"

#include "tiles/writer.h"

using namespace geo;
using namespace net::http::server;
using namespace rocksdb;
using namespace rocksdb::spatial;

constexpr char kDatabasePath[] = "spatial";

using proj = geo::webmercator4096;

void checked(Status&& status) {
  if (!status.ok()) {
    std::cout << "error: " << status.ToString() << std::endl;
    std::exit(1);
  }
}

int main() {
  if (!boost::filesystem::is_directory(kDatabasePath)) {
    std::cout << "database missing!" << std::endl;
    return 1;
  }

  SpatialDB* db;
  checked(SpatialDB::Open(SpatialDBOptions(), kDatabasePath, &db, true));

  boost::asio::io_service ios;
  server server{ios};

  query_router router;
  router.route("OPTIONS", ".*", [](auto const&, auto cb) {
    reply rep = reply::stock_reply(reply::ok);
    add_cors_headers(rep);
    cb(rep);
  });

  // z, x, y
  router.route("GET", "^\\/(\\d+)\\/(\\d+)\\/(\\d+).mvt$",
               [&](auto const& req, auto cb) {
                 std::cout << "received a request: " << req.uri << std::endl;

    auto const bounds = proj::tile_bounds(std::stoul(req.path_params[1]),
                                                std::stoul(req.path_params[2]),
                                                std::stoul(req.path_params[0]);

    Cursor* cur = db->Query(ReadOptions(), bbox(bounds),
                            "zoom10");

    std::vector<meters> found;
    while (cur->Valid()) {
      // std::cout << "#" << std::string(cur->blob().data(), cur->blob().size())
      //           << std::endl;

      auto mem = from_slice(cur->blob());
      found.emplace_back(mem.at(1), mem.at(2));

      cur->Next();
    }

    reply rep = reply::stock_reply(reply::ok);
    rep.content = tiles::make_tile(meters);
    add_cors_headers(rep);
    cb(rep);
               });

  server.listen("0.0.0.0", "8888", router);

  io_service_shutdown shutd(ios);
  shutdown_handler<io_service_shutdown> shutdown(ios, shutd);

  while (true) {
    try {
      ios.run();
      break;
    } catch (std::exception const& e) {
      std::cout << "unhandled error: " << e.what() << std::endl;
    } catch (...) {
      std::cout << "unhandled unknown error" << std::endl;
    }
  }
}