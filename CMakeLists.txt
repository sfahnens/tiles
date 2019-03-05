cmake_minimum_required(VERSION 3.0)
project(tiles)

include_directories(include)

find_package(Boost COMPONENTS system filesystem thread iostreams REQUIRED)
find_package(Threads)

include_directories(include)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

set(TILES_CXX_FLAGS "-std=c++17 -Wall -Wextra")

add_definitions(-DPROTOZERO_USE_VIEW=std::string_view)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/deps")
  add_subdirectory(deps)
endif()

file(GLOB_RECURSE tiles-files
  src/db/*.cc
  src/fixed/*.cc
  src/mvt/*.cc
  src/tile_database.cc
  src/perf_counter.cc
  src/util.cc
)

add_library(tiles EXCLUDE_FROM_ALL ${tiles-files})
set_target_properties(tiles PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles
  ${Boost_LIBRARIES}
  clipper
  fmt
  geo
  lmdb
  osmium
  utl
  concurrentqueue
  conf
  protozero
  sol2
  miniz
)

file(GLOB_RECURSE tiles-import-files src/osm/*.cc)
file(GLOB tiles-import-main-files src/import.cc)

add_executable(tiles-import EXCLUDE_FROM_ALL
  ${tiles-import-main-files}
  ${tiles-import-files})
set_target_properties(tiles-import PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles-import
  ${Boost_LIBRARIES}
  conf
  geo
  lmdb
  lua
  tiles
  zlib
)

file(GLOB_RECURSE tiles-server-files src/server.cc)

add_executable(tiles-server EXCLUDE_FROM_ALL ${tiles-server-files})
set_target_properties(tiles-server PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles-server
  ${Boost_LIBRARIES}
  geo
  lmdb
  net-http_server
  tiles
  zlib
)


file(GLOB_RECURSE tiles-benchmark-files src/benchmark.cc)

add_executable(tiles-benchmark EXCLUDE_FROM_ALL ${tiles-benchmark-files})
set_target_properties(tiles-benchmark PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles-benchmark
  ${Boost_LIBRARIES}
  tiles
  lmdb
  geo
)

file(GLOB_RECURSE tiles-test-files test/*.cc)

add_executable(tiles-test EXCLUDE_FROM_ALL ${tiles-test-files})
set_target_properties(tiles-test PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles-test tiles)
