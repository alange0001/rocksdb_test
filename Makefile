
all: build
	cd build && make -j6

build: CMakeLists.txt
	mkdir build || true
	cd build && cmake .. && make clean

clean:
	cd build && make clean
