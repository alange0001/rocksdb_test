
WORKSPACE ?= $(shell cd ..; pwd)
ROCKSDB_PATH ?= $(WORKSPACE)/rocksdb
SPDLOG_PATH ?= $(WORKSPACE)/spdlog
FMT_PATH ?= $(WORKSPACE)/fmt
SRC = $(shell find -P -name '*.cc' -or -name '*.h' |cut -c 3-)

all: bazel-bin/rocksdb_test

bazel-bin/rocksdb_test: WORKSPACE BUILD $(SRC)
	echo '' >vars.bzl
	echo ROCKSDB_PATH = \"$(ROCKSDB_PATH)\" >>vars.bzl
	echo SPDLOG_PATH = \"$(SPDLOG_PATH)\" >>vars.bzl
	echo FMT_PATH = \"$(FMT_PATH)\" >>vars.bzl
	bazel build :rocksdb_test

clean:
	rm vars.bzl
	bazel clean
