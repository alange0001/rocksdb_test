workspace(name = "rocksdb_test")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load(":vars.bzl", "ROCKSDB_PATH", "SPDLOG_PATH", "FMT_PATH")

git_repository(
    name = "com_github_gflags_gflags",
    remote = "https://github.com/gflags/gflags.git",
    tag = "v2.2.2",
)

new_local_repository(
	name = "rocksdb",
	path = ROCKSDB_PATH,
    build_file_content = """
#cc_library(
#        name = "util",
#        hdrs = glob(["util/*.h"]),
#        strip_include_prefix = "util/",
#        include_prefix = "util",
#        visibility = ["//visibility:public"],
#        )
cc_library(
        name = "main",
        srcs = ['librocksdb.a'],
        hdrs = glob(["include/rocksdb/**"]),
        strip_include_prefix = "include/",
        linkopts = ["-lz", "-llz4", "-lzstd", "-lsnappy", "-lbz2", "-ldl"],
        visibility = ["//visibility:public"],
        #deps = [":util"],
        )""",
)

new_local_repository(
	name = "spdlog",
	path = SPDLOG_PATH,
    build_file_content = """
cc_library(
        name = "main",
        srcs = ['build/libspdlog.a'],
        hdrs = glob(["include/spdlog/**"]),
        strip_include_prefix = "include/",
        visibility = ["//visibility:public"],
        )""",
)

new_local_repository(
	name = "fmt",
	path = FMT_PATH,
    build_file_content = """
cc_library(
        name = "main",
        srcs = ['build/libfmt.a'],
        hdrs = glob(["include/fmt/**"]),
        strip_include_prefix = "include/",
        visibility = ["//visibility:public"],
        )""",
)
