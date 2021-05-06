
bin: build 3rd-party-gflags 3rd-party-fmt 3rd-party-spdlog 3rd-party-alutils 3rd-party-nlohmann
	echo '#pragma once' > version.h.new
	echo '#define ROCKSDB_TEST_VERSION "1.12"' >> version.h.new
	test -f "version.h" || cp -f version.h.new version.h
	test "$(shell md5sum version.h |cut -d' ' -f1 )x == $(shell md5sum version.h.new |cut -d' ' -f1 )x" || cp -f version.h.new version.h 
	cd build && make -j6

all: bin AppImage

AppImage: build/rocksdb_test_helper.AppImage

build/rocksdb_test_helper.AppImage: rocksdb_test_helper.AppImageBuilder.yml rocksdb_test_helper.py
	cd build && [ -f rocksdb_test_helper.AppImageBuilder.yml ] || ln -s ../rocksdb_test_helper.AppImageBuilder.yml
	cd build && rm -f rocksdb_test_helper-*-x86_64.AppImage rocksdb_test_helper.AppImage
	cd build && appimage-builder --recipe=rocksdb_test_helper.AppImageBuilder.yml --skip-test
	cd build && ln -s rocksdb_test_helper-*-x86_64.AppImage rocksdb_test_helper.AppImage 

build: CMakeLists.txt
	mkdir build || true
	cd build && cmake .. && make clean

release: build/rocksdb_test build/rocksdb_test_helper.AppImage plot/exp_db.ipynb
	mkdir -p release/plot/exp_db || true
	cp build/rocksdb_test_helper.AppImage release/rocksdb_test_helper
	cp build/rocksdb_test release/
	cp files/rocksdb-6.8-db_bench.options release/rocksdb.options
	cp plot/plot.py release/plot/
	cp plot/exp_db/*.out release/plot/exp_db/
	cp plot/exp_db.ipynb release/plot/

3rd-party:
	mkdir 3rd-party || true

3rd-party-gflags: 3rd-party/gflags/build/lib/libgflags.a

3rd-party/gflags/build/lib/libgflags.a: 3rd-party
	test -d 3rd-party/gflags || git clone -b "v2.2.2" --depth 1 -- https://github.com/gflags/gflags.git 3rd-party/gflags
	mkdir 3rd-party/gflags/build || true
	cd 3rd-party/gflags/build && cmake ..
	cd 3rd-party/gflags/build && make

3rd-party-fmt: 3rd-party/fmt/build/libfmt.a

3rd-party/fmt/build/libfmt.a: 3rd-party
	test -d 3rd-party/fmt || git clone -b "6.2.0" --depth 1 -- https://github.com/fmtlib/fmt.git 3rd-party/fmt
	mkdir 3rd-party/fmt/build || true
	cd 3rd-party/fmt/build && cmake ..
	cd 3rd-party/fmt/build && make

3rd-party-spdlog: 3rd-party/spdlog/build/libspdlog.a

3rd-party/spdlog/build/libspdlog.a: 3rd-party
	test -d 3rd-party/spdlog || git clone -b "v1.x" --depth 1 -- https://github.com/gabime/spdlog.git 3rd-party/spdlog
	mkdir 3rd-party/spdlog/build || true
	cd 3rd-party/spdlog/build && cmake ..
	cd 3rd-party/spdlog/build && make

3rd-party-alutils: 3rd-party/alutils/build/libalutils.a

3rd-party/alutils/build/libalutils.a: 3rd-party
	test -d 3rd-party/alutils || git clone -b "rocksdb_test-v1.11" --depth 1 -- https://github.com/alange0001/alutils.git 3rd-party/alutils
	cd 3rd-party/alutils && make

3rd-party-nlohmann: 3rd-party/nlohmann/nlohmann/json.hpp

3rd-party/nlohmann/nlohmann/json.hpp: 3rd-party
	test -d 3rd-party/nlohmann/nlohmann || mkdir -p 3rd-party/nlohmann/nlohmann
	wget "https://github.com/nlohmann/json/raw/v3.9.1/single_include/nlohmann/json.hpp" -O 3rd-party/nlohmann/nlohmann/json.hpp

clean-release:
	rm -fr release

clean-3rd-party:
	rm -fr 3rd-party

clean: clean-release
	rm -fr build
