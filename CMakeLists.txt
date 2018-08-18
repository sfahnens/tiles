cmake_minimum_required(VERSION 3.0)
project(tiles)

include_directories(include)

find_package(Boost COMPONENTS system filesystem thread iostreams REQUIRED)
find_package(Threads)

include_directories(include)
if (NOT ZLIB_FOUND)
  find_package(ZLIB REQUIRED)
  include_directories(${ZLIB_INCLUDE_DIRS})
endif()

link_directories(${Boost_LIBRARY_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

set(TILES_CXX_FLAGS "-std=c++17 -Wall -Wextra")

SET(TILES_STANDALONE ON CACHE BOOL "get dependencies from external_lib")

add_definitions(-DPROTOZERO_USE_VIEW=std::string_view)

if(TILES_STANDALONE)
  include_directories(SYSTEM external_lib/concurrentqueue)
  include_directories(SYSTEM external_lib/conf/include)
  include_directories(SYSTEM external_lib/geo/include)
  include_directories(SYSTEM external_lib/lua-5.1/src)
  include_directories(SYSTEM external_lib/lz4/lib)
  include_directories(SYSTEM external_lib/miniz)
  include_directories(SYSTEM external_lib/net/include)
  include_directories(SYSTEM external_lib/osmium/include)
  include_directories(SYSTEM external_lib/lmdb/include)
  include_directories(SYSTEM external_lib/lmdb/lib)
  include_directories(SYSTEM external_lib/sparsehash/src)
  include_directories(SYSTEM external_lib/clipper/include)
  include_directories(SYSTEM external_lib/sol2)
  include_directories(SYSTEM external_lib/utl/include)

  add_subdirectory(external_lib/conf EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/geo EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/lua-5.1 EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/lz4 EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/net EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/lmdb EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/clipper EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/fmt EXCLUDE_FROM_ALL)
endif()

file(GLOB_RECURSE tiles-files
  src/db/*.cc
  src/fixed/*.cc
  src/mvt/*.cc
  src/tile_database.cc
  src/perf_counter.cc
  src/util.cc
)

add_library(tiles ${tiles-files})
set_target_properties(tiles PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles
  ${Boost_LIBRARIES}
  ${ZLIB_LIBRARIES}
  clipper
  fmt
  geo
  lmdb
)


if(TILES_STANDALONE)
  file(GLOB_RECURSE tiles-import-files src/osm/*.cc)
  file(GLOB tiles-import-main-files src/import.cc)

  add_executable(tiles-import ${tiles-import-main-files} ${tiles-import-files})
  set_target_properties(tiles-import PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
  target_link_libraries(tiles-import
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    conf
    geo
    lmdb
    lua
    tiles
  )

  file(GLOB_RECURSE tiles-server-files src/server.cc)

  add_executable(tiles-server ${tiles-server-files})
  set_target_properties(tiles-server PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
  target_link_libraries(tiles-server
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    geo
    lmdb
    net-http_server
    tiles
  )


  file(GLOB_RECURSE tiles-benchmark-files src/benchmark.cc)

  add_executable(tiles-benchmark ${tiles-benchmark-files})
  set_target_properties(tiles-benchmark PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
  target_link_libraries(tiles-benchmark
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    tiles
    lmdb
    geo
  )
endif()

file(GLOB_RECURSE tiles-test-files test/*.cc)

add_executable(tiles-test ${tiles-test-files})
set_target_properties(tiles-test PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles-test tiles)
