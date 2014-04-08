#run cmake with -DCMAKE_TOOLCHAIN_FILE=unicos_target.toolchain for uniCOS compilation
#run cmake with -DCMAKE_BUILD_TYPE=Debug for debug builds and Release for release builds

project(pFTP)

if( "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}" )
    message (FATAL_ERROR "Don't run cmake inside source folder.\n Instead create a folder \"build\" and run \"cmake ..\" from there")
endif()

cmake_minimum_required(VERSION 2.6)

#SET(CMAKE_C_FLAGS "-std=c99 -fmessage-length=0")

string(FIND "${CMAKE_TOOLCHAIN_FILE}" "unicos" UNICOS_TARGET)
if(${UNICOS_TARGET} EQUAL -1)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(PGM REQUIRED openpgm-5.1)
    include_directories(${PGM_INCLUDE_DIRS})
    link_directories(${PGM_LIBRARY_DIRS})
    add_definitions(${PGM_CLFAGS_OTHER})
else()
    include_directories("/home/user/cu_build1/output/target/usr/include/pgm-5.1")
    link_directories("/home/user/cu_build1/output/target/usr/lib/")
    set(PGM_LIBRARIES "-lpgm")
endif()

set(PTHREAD_LIBRARIES "-lpthread")

set(SERVER_SOURCE
    pftp.c
    pftp_common.h
    pftp_common.c
)

set(CLIENT_SOURCE
    pftpd.c
    pftp_common.h
    pftp_common.c
)

add_executable(pftp ${SERVER_SOURCE})
target_link_libraries(pftp ${PGM_LIBRARIES} ${PTHREAD_LIBRARIES})

add_executable(pftpd ${CLIENT_SOURCE})
target_link_libraries(pftpd ${PGM_LIBRARIES} ${PTHREAD_LIBRARIES})

add_executable(purinsend purinsend.c)
target_link_libraries(purinsend ${PGM_LIBRARIES})

add_executable(purinrecv purinrecv.c)
target_link_libraries(purinrecv ${PGM_LIBRARIES})
