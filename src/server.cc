#include <exception>
#include <iomanip>
#include <iostream>

#include "boost/filesystem.hpp"

#include "net/http/server/enable_cors.hpp"
#include "net/http/server/query_router.hpp"
#include "net/http/server/server.hpp"
#include "net/http/server/shutdown_handler.hpp"

#include "tiles/db/get_tile.h"
#include "tiles/db/tile_database.h"

using namespace net::http::server;

int main() {
  lmdb::env db_env = tiles::make_tile_database("./");
  tiles::tile_db_handle handle{db_env};

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
                 try {
                   std::cout << "received a request: " << req.uri << std::endl;

                   auto const tile = geo::tile{
                       static_cast<uint32_t>(std::stoul(req.path_params[1])),
                       static_cast<uint32_t>(std::stoul(req.path_params[2])),
                       static_cast<uint32_t>(std::stoul(req.path_params[0]))};

                   reply rep = reply::stock_reply(reply::ok);
                   rep.content = tiles::get_tile(handle, tile);

                   if(rep.content.empty()) {
                    rep.status = reply::no_content;
                   }

                   add_cors_headers(rep);
                   cb(rep);
                 } catch (std::exception const& e) {
                   std::cout << "unhandled error: " << e.what() << std::endl;
                 } catch (...) {
                   std::cout << "unhandled unknown error" << std::endl;
                 }
                 std::cout << "done:" << req.uri << std::endl;
               });

  server.listen("0.0.0.0", "8888", router);

  io_service_shutdown shutd(ios);
  shutdown_handler<io_service_shutdown> shutdown(ios, shutd);

  std::cout << ">>> tiles-server up and running!" << std::endl;
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
