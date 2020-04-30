
cc_library(
    name = "args",
    srcs = ["args.cc"],
    hdrs = ["args.h"],
    copts = ["-std=c++11"],
    deps = ["@com_github_gflags_gflags//:gflags", "@spdlog//:main", "@fmt//:main"],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    copts = ["-std=c++11"],
)

cc_binary(
    name = "rocksdb_test",
    srcs = ["rocksdb_test.cc"],
    copts = ["-std=c++11"],
    linkopts = ["-std=c++11"],
    deps = [":args", ":util", "@rocksdb//:main", "@spdlog//:main", "@fmt//:main"],
)
