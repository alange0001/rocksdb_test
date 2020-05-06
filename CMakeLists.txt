cmake_minimum_required (VERSION 2.6)

project (rocksdb_test)

#################################################################

set(WORKSPACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(ROCKSDB_DIR "${WORKSPACE}/rocksdb")
set(FMT_DIR "${WORKSPACE}/fmt")
set(SPDLOG_DIR "${WORKSPACE}/spdlog")

set(gflags_DIR "${WORKSPACE}/gflags/build")
find_package(gflags REQUIRED)
add_definitions(-DGFLAGS=1)
include_directories(${gflags_INCLUDE_DIR})
list(APPEND THIRDPARTY_LIBS gflags)

include_directories("${FMT_DIR}/include")
list(APPEND THIRDPARTY_LIBS "${FMT_DIR}/build/libfmt.a")

include_directories ("${SPDLOG_DIR}/include")
list(APPEND THIRDPARTY_LIBS "${SPDLOG_DIR}/build/libspdlog.a")

include_directories ("${ROCKSDB_DIR}/include")
list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/librocksdb.a")
list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libbz2.a")
list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/liblz4.a")
list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libz.a")
list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libzstd.a")
list(APPEND THIRDPARTY_LIBS "${ROCKSDB_DIR}/libsnappy.a")

list(APPEND THIRDPARTY_LIBS pthread)
list(APPEND THIRDPARTY_LIBS dl)

#################################################################

add_executable(rocksdb_test rocksdb_test.cc util.cc args.cc)
target_link_libraries(rocksdb_test ${THIRDPARTY_LIBS})

