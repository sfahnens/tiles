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

include_directories(SYSTEM external_lib/geo/include)
include_directories(SYSTEM external_lib/net/include)
include_directories(SYSTEM external_lib/miniz)
include_directories(SYSTEM external_lib/osmium/include)
include_directories(SYSTEM external_lib/rocksdb/include)
include_directories(SYSTEM external_lib/rocksdb/external_lib/snappy)

add_subdirectory(external_lib/net EXCLUDE_FROM_ALL)
add_subdirectory(external_lib/geo EXCLUDE_FROM_ALL)
add_subdirectory(external_lib/rocksdb EXCLUDE_FROM_ALL)

add_executable(loader src/loader.cc)
set_target_properties(loader PROPERTIES COMPILE_FLAGS "-std=c++14 -Wall -Wextra")
target_link_libraries(loader ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} rocksdb geo)


add_executable(server src/server.cc src/mvt/builder.cc src/mvt/encoder.cc)
set_target_properties(server PROPERTIES COMPILE_FLAGS "-std=c++14 -Wall -Wextra")
target_link_libraries(server ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} rocksdb geo net-http_server)
