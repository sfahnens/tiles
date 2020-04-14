cmake_minimum_required(VERSION 3.13)
project(tiles)

find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(Threads)

link_directories(${Boost_LIBRARY_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

if (MSVC)
  set(TILES_WARNINGS "/W4")
else()
  set(TILES_WARNINGS "-Wall" "-Wextra")
endif()

include(cmake/pkg.cmake)

file(GLOB_RECURSE tiles-files
  src/db/*.cc
  src/feature/*.cc
  src/fixed/*.cc
  src/mvt/*.cc
  src/tile_database.cc
  src/perf_counter.cc
  src/util.cc
)

add_library(tiles STATIC ${tiles-files})
set_property(TARGET tiles PROPERTY CXX_STANDARD 17)
target_compile_options(tiles PRIVATE ${TILES_WARNINGS})
target_include_directories(tiles PUBLIC include)
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
  zlibstatic
)

file(GLOB_RECURSE tiles-import-files src/osm/*.cc)
file(GLOB tiles-import-main-files src/import.cc)

add_executable(tiles-import EXCLUDE_FROM_ALL
  ${tiles-import-main-files}
  ${tiles-import-files})
set_property(TARGET tiles-import PROPERTY CXX_STANDARD 17)
target_compile_options(tiles-import PRIVATE ${TILES_WARNINGS})
target_include_directories(tiles-import PUBLIC include)
target_link_libraries(tiles-import
  ${Boost_LIBRARIES}
  conf
  geo
  lmdb
  lua
  tiles
)

add_executable(tiles-server EXCLUDE_FROM_ALL src/server.cc)
set_property(TARGET tiles-server PROPERTY CXX_STANDARD 17)
target_compile_options(tiles-server PRIVATE ${TILES_WARNINGS})
target_include_directories(tiles-server PUBLIC include)
target_link_libraries(tiles-server
  ${Boost_LIBRARIES}
  conf
  geo
  lmdb
  tiles
)

file(GLOB_RECURSE tiles-server-res "${CMAKE_CURRENT_SOURCE_DIR}/client/*.*")
create_resource("${CMAKE_CURRENT_SOURCE_DIR}/client/" "${tiles-server-res}" tiles_server_res)
target_link_libraries(tiles-server
  tiles_server_res
  tiles_server_res-res
)

target_link_libraries(tiles-server
  pbf_sdf_fonts_res
  pbf_sdf_fonts_res-res
)

file(GLOB_RECURSE tiles-benchmark-files src/benchmark.cc)

add_executable(tiles-benchmark EXCLUDE_FROM_ALL ${tiles-benchmark-files})
set_property(TARGET tiles-benchmark PROPERTY CXX_STANDARD 17)
target_compile_options(tiles-benchmark PRIVATE ${TILES_WARNINGS})
target_include_directories(tiles-benchmark PUBLIC include)
target_link_libraries(tiles-benchmark
  ${Boost_LIBRARIES}
  tiles
  lmdb
  geo
)

file(GLOB_RECURSE tiles-test-files test/*.cc src/osm/hybrid_node_idx.cc)

add_executable(tiles-test EXCLUDE_FROM_ALL ${tiles-test-files})
set_property(TARGET tiles-test PROPERTY CXX_STANDARD 17)
target_compile_options(tiles-test PRIVATE ${TILES_WARNINGS})
target_include_directories(tiles-test PUBLIC include)
target_link_libraries(tiles-test tiles Catch2)
