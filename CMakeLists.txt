project (pFTP)

if( "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}" )
    message (FATAL_ERROR "Don't run cmake inside source folder.\n Instead create a folder \"build\" and run cmake .. from there")
endif()

cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig REQUIRED)

pkg_check_modules(PGM REQUIRED openpgm-5.1)
include_directories(${PGM_INCLUDE_DIRS})
link_directories(${PGM_LIBRARY_DIRS})
add_definitions(${PGM_CLFAGS_OTHER})

add_executable(pftp pftp.c)
target_link_libraries(pftp ${PGM_LIBRARIES})
