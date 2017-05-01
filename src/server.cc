#include <exception>
#include <iomanip>
#include <iostream>

#include "boost/filesystem.hpp"

#include "rocksdb/utilities/spatial_db.h"

#include "net/http/server/enable_cors.hpp"
#include "net/http/server/query_router.hpp"
#include "net/http/server/server.hpp"
#include "net/http/server/shutdown_handler.hpp"

#include "tiles/mvt/builder.h"
#include "tiles/mvt/dummy.h"
#include "tiles/mvt/tile_spec.h"
#include "tiles/util.h"

using namespace tiles;
using namespace net::http::server;
using namespace rocksdb;
using namespace rocksdb::spatial;

constexpr char kDatabasePath[] = "spatial";

void checked(Status&& status) {
  if (!status.ok()) {
    std::cout << "error: " << status.ToString() << std::endl;
    std::exit(1);
  }
}

int main() {
  // if (!boost::filesystem::is_directory(kDatabasePath)) {
  //   std::cout << "database missing!" << std::endl;
  //   return 1;
  // }

  // SpatialDB* db;
  // checked(SpatialDB::Open(SpatialDBOptions(), kDatabasePath, &db, true));

  // boost::asio::io_service ios;
  // server server{ios};

  // query_router router;
  // router.route("OPTIONS", ".*", [](auto const&, auto cb) {
  //   reply rep = reply::stock_reply(reply::ok);
  //   add_cors_headers(rep);
  //   cb(rep);
  // });

  // // z, x, y
  // router.route("GET", "^\\/(\\d+)\\/(\\d+)\\/(\\d+).mvt$", [&](auto const& req,
  //                                                              auto cb) {
  //   try {
  //     std::cout << "received a request: " << req.uri << std::endl;

  //     auto const spec =
  //         tile_spec{static_cast<uint32_t>(std::stoul(req.path_params[1])),
  //                   static_cast<uint32_t>(std::stoul(req.path_params[2])),
  //                   static_cast<uint32_t>(std::stoul(req.path_params[0]))};
  //     tile_builder tb{spec};

  //     std::cout << "merc bounds: " << spec.merc_bounds_.minx_ << " "
  //               << spec.merc_bounds_.maxx_ << "|" << spec.merc_bounds_.miny_
  //               << " " << spec.merc_bounds_.maxy_ << std::endl;

  //     std::cout << "pixl bounds: " << spec.pixel_bounds_.minx_ << " "
  //               << spec.pixel_bounds_.maxx_ << "|" << spec.pixel_bounds_.miny_
  //               << " " << spec.pixel_bounds_.maxy_ << std::endl;

  //     Cursor* cur =
  //         db->Query(ReadOptions(), bbox(spec.merc_bounds_), spec.z_str());
  //     while (cur->Valid()) {
  //       // std::cout << "found feature" << std::endl;
  //       tb.add_feature(cur->feature_set(), cur->blob());
  //       cur->Next();
  //       // break;
  //     }

  //     reply rep = reply::stock_reply(reply::ok);
  //     rep.content = tb.finish();
  //     // rep.content = make_tile();
  //     add_cors_headers(rep);
  //     cb(rep);
  //   } catch (std::exception const& e) {
  //     std::cout << "unhandled error: " << e.what() << std::endl;
  //   } catch (...) {
  //     std::cout << "unhandled unknown error" << std::endl;
  //   }

  // });

  // server.listen("0.0.0.0", "8888", router);

  // io_service_shutdown shutd(ios);
  // shutdown_handler<io_service_shutdown> shutdown(ios, shutd);

  // while (true) {
  //   try {
  //     ios.run();
  //     break;
  //   } catch (std::exception const& e) {
  //     std::cout << "unhandled error: " << e.what() << std::endl;
  //   } catch (...) {
  //     std::cout << "unhandled unknown error" << std::endl;
  //   }
  // }
}
