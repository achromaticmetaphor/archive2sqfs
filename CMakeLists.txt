cmake_minimum_required(VERSION 3.1)

find_package(LibArchive REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(${LibArchive_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})

add_executable(archive2sqfs archive2sqfs
  compressor_zlib compressor_zstd
  dirtree_dir dirtree_reg dirtree_write
  metadata_writer pending_write sqsh_writer)

target_link_libraries(archive2sqfs ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(archive2sqfs ${LibArchive_LIBRARIES})
target_link_libraries(archive2sqfs ${ZLIB_LIBRARIES})

if (USE_BOOST)
  find_package(Boost 1.30.0 REQUIRED COMPONENTS filesystem)
  include_directories(${Boost_INCLUDE_DIRS})
  target_link_libraries(archive2sqfs ${Boost_LIBRARIES})
  target_compile_definitions(archive2sqfs PRIVATE LSL_USE_BOOST=1)
endif()

if (USE_POSIX)
  target_compile_definitions(archive2sqfs PRIVATE LSL_USE_POSIX=1)
  target_compile_definitions(archive2sqfs PRIVATE _POSIX_C_SOURCE=200809L)
endif()

if (USE_ZSTD)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ZSTD REQUIRED libzstd)
  target_compile_definitions(archive2sqfs PRIVATE LSL_ENABLE_COMP_zstd=1)

  include_directories(${ZSTD_INCLUDE_DIRS})
  link_directories(${ZSTD_LIBRARY_DIRS})
  target_compile_definitions(archive2sqfs PRIVATE ${ZSTD_CFLAGS_OTHER})
  target_link_libraries(archive2sqfs ${ZSTD_LIBRARIES})
endif()

set_property(TARGET archive2sqfs PROPERTY CXX_STANDARD 14)
set_property(TARGET archive2sqfs PROPERTY CXX_STANDARD_REQUIRED ON)
