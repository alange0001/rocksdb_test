cmake_minimum_required (VERSION 2.6)

project (rocksdb_test)

#################################################################

set(WORKSPACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

# https://github.com/gflags/gflags.git
set(gflags_DIR "${WORKSPACE}/gflags/build")
find_package(gflags REQUIRED)
add_definitions(-DGFLAGS=1)
include_directories(${gflags_INCLUDE_DIR})
list(APPEND THIRDPARTY_LIBS gflags)

# https://github.com/fmtlib/fmt.git
set(FMT_DIR "${WORKSPACE}/fmt")
include_directories("${FMT_DIR}/include")
list(APPEND THIRDPARTY_LIBS "${FMT_DIR}/build/libfmt.a")

# https://github.com/gabime/spdlog.git
set(SPDLOG_DIR "${WORKSPACE}/spdlog")
include_directories ("${SPDLOG_DIR}/include")
list(APPEND THIRDPARTY_LIBS "${SPDLOG_DIR}/build/libspdlog.a")

# https://github.com/alange0001/alutils.git
set(ALUTILS_DIR "${WORKSPACE}/alutils")
include_directories ("${ALUTILS_DIR}/include")
list(APPEND THIRDPARTY_LIBS "${ALUTILS_DIR}/build/libalutils.a")

# https://github.com/facebook/rocksdb.git
#set(ROCKSDB_DIR "${WORKSPACE}/rocksdb")
#include_directories ("${ROCKSDB_DIR}/include")
#list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/librocksdb.a")
#list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libbz2.a")
#list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/liblz4.a")
#list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libz.a")
#list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libzstd.a")
#list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libsnappy.a")

list(APPEND THIRDPARTY_LIBS pthread)
list(APPEND THIRDPARTY_LIBS dl)

#################################################################

add_executable(rocksdb_test rocksdb_test.cc util.cc args.cc)
target_link_libraries(rocksdb_test ${THIRDPARTY_LIBS})
set_property(TARGET rocksdb_test PROPERTY CXX_STANDARD 17)

add_executable(access_time3 access_time3.cc access_time3_args.cc util.cc)
target_link_libraries(access_time3 ${THIRDPARTY_LIBS})
set_property(TARGET access_time3 PROPERTY CXX_STANDARD 17)

add_executable(test test.cc util.cc)
target_link_libraries(test ${THIRDPARTY_LIBS})
set_property(TARGET test PROPERTY CXX_STANDARD 17)
