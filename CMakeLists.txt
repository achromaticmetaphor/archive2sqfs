cmake_minimum_required(VERSION 3.1)

find_package(LibArchive REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(${LibArchive_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})

add_executable(archive2sqfs archive2sqfs compressor dirtree_dir dirtree_reg dirtree_write metadata_writer sqsh_writer)

target_link_libraries(archive2sqfs ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(archive2sqfs ${LibArchive_LIBRARIES})
target_link_libraries(archive2sqfs ${ZLIB_LIBRARIES})

set_property(TARGET archive2sqfs PROPERTY CXX_STANDARD 14)
set_property(TARGET archive2sqfs PROPERTY CXX_STANDARD_REQUIRED ON)
