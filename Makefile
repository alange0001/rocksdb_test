
bin: build/rocksdb_test

all: bin AppImage

AppImage: rocksdb_test.AppImageBuilder.yml build/rocksdb_test
	cd build && [ -f rocksdb_test.AppImageBuilder.yml ] || ln -s ../rocksdb_test.AppImageBuilder.yml
	cd build && rm -f rocksdb_test-*-x86_64.AppImage rocksdb_test.AppImage
	cd build && appimage-builder --recipe=rocksdb_test.AppImageBuilder.yml --skip-test
	cd build && ln -s rocksdb_test-*-x86_64.AppImage rocksdb_test.AppImage 

build: CMakeLists.txt
	mkdir build || true
	cd build && cmake .. && make clean

build/rocksdb_test: build
	cd build && make -j6

clean:
	rm -fr build

clean-AppImage:
	cd build && rm -f rocksdb_test-*-x86_64.AppImage rocksdb_test.AppImage
