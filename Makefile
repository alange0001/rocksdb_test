
bin: build
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

clean-release:
	rm -fr release

clean: clean-release
	rm -fr build
