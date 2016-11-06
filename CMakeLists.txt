cmake_minimum_required(VERSION 3.1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(ZLIB REQUIRED zlib)

include_directories(${LIBARCHIVE_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})

link_directories(${LIBARCHIVE_LIBRARY_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})
link_directories(${ZLIB_LIBRARY_DIRS})

add_definitions(${LIBARCHIVE_CFLAGS_OTHER})
add_definitions(${GLIB_CFLAGS_OTHER})
add_definitions(${ZLIB_CFLAGS_OTHER})

add_executable(archive2sqfs dirtree.c dirtree_dir.c dirtree_reg.c dirtree_write.c archive2sqfs.c)

target_link_libraries(archive2sqfs ${LIBARCHIVE_LIBRARIES})
target_link_libraries(archive2sqfs ${GLIB_LIBRARIES})
target_link_libraries(archive2sqfs ${ZLIB_LIBRARIES})

set_property(TARGET archive2sqfs PROPERTY C_STANDARD 99)
set_property(TARGET archive2sqfs PROPERTY C_STANDARD_REQUIRED ON)
