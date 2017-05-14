cmake_minimum_required(VERSION 3.0)
project(tiles)

include_directories(include)

find_package(Boost COMPONENTS system filesystem thread REQUIRED)
find_package(Threads)

include_directories(include)
if (NOT ZLIB_FOUND)
  find_package(ZLIB REQUIRED)
  include_directories(${ZLIB_INCLUDE_DIRS})
endif()

link_directories(${Boost_LIBRARY_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

set(TILES_CXX_FLAGS "-std=c++14 -Wall -Wextra")

SET(TILES_STANDALONE ON CACHE BOOL "get dependencies from external_lib")

if(TILES_STANDALONE)
  include_directories(SYSTEM external_lib/conf/include)
  include_directories(SYSTEM external_lib/geo/include)
  include_directories(SYSTEM external_lib/lua-5.1/src)
  include_directories(SYSTEM external_lib/lz4/lib)
  include_directories(SYSTEM external_lib/miniz)
  include_directories(SYSTEM external_lib/net/include)
  include_directories(SYSTEM external_lib/osmium/include)
  include_directories(SYSTEM external_lib/rocksdb/include)
  include_directories(SYSTEM external_lib/rocksdb/external_lib/snappy)
  include_directories(SYSTEM external_lib/sparsehash/src)
  include_directories(SYSTEM external_lib/sol2)
  include_directories(SYSTEM external_lib/utl/include)

  add_subdirectory(external_lib/conf EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/geo EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/lua-5.1 EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/lz4 EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/net EXCLUDE_FROM_ALL)
  add_subdirectory(external_lib/rocksdb EXCLUDE_FROM_ALL)
endif()

file(GLOB_RECURSE tiles-files src/fixed/*.cc src/mvt/*.cc src/tile_database.cc)

add_library(tiles ${tiles-files})
set_target_properties(tiles PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles
  ${Boost_LIBRARIES}
  ${ZLIB_LIBRARIES}
  rocksdb
  geo
)


if(TILES_STANDALONE)
  file(GLOB_RECURSE tiles-import-files src/import.cc src/loader/*.cc)

  add_executable(tiles-import ${tiles-import-files})
  set_target_properties(tiles-import PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
  target_link_libraries(tiles-import
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    tiles
    rocksdb
    lua
    geo
  )

  file(GLOB_RECURSE tiles-server-files src/server.cc)

  add_executable(tiles-server ${tiles-server-files})
  set_target_properties(tiles-server PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
  target_link_libraries(tiles-server
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    tiles
    rocksdb
    geo
    net-http_server
  )

  add_executable(luatest src/luatest.cc)
  set_target_properties(luatest PROPERTIES COMPILE_FLAGS "-std=c++14 -Wall -Wextra")
  target_link_libraries(luatest ${Boost_LIBRARIES} lua)

  # add_executable(area_test external_lib/osmium/examples/osmium_area_test.cpp)
  # set_target_properties(area_test PROPERTIES COMPILE_FLAGS "-std=c++14 -Wall -Wextra")
  # target_link_libraries(area_test ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

  add_executable(osmtest src/osmtest.cc)
  set_target_properties(osmtest PROPERTIES COMPILE_FLAGS "-std=c++14 -Wall -Wextra")
  target_link_libraries(osmtest
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    geo
  )

  add_executable(geotest src/geotest.cc)
  set_target_properties(geotest PROPERTIES COMPILE_FLAGS "-std=c++14 -Wall -Wextra")
  target_link_libraries(geotest ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} geo)
endif()

file(GLOB_RECURSE tiles-test-files test/*.cc)

add_executable(tiles-test ${tiles-test-files})
set_target_properties(tiles-test PROPERTIES COMPILE_FLAGS ${TILES_CXX_FLAGS})
target_link_libraries(tiles-test tiles)
