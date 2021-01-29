
WORKSPACE = $(shell echo "${PWD}")
ROCKSDB_DST = $(WORKSPACE)/opt/rocksdb
ROCKSDB_SRC ?= $(WORKSPACE)/../../rocksdb
ROCKSDB_TEST_DST = $(WORKSPACE)/opt/rocksdb_test
ROCKSDB_TEST_SRC ?= $(WORKSPACE)/..
YCSB_SRC ?= $(WORKSPACE)/../../YCSB
YCSB_DST = $(WORKSPACE)/opt/YCSB

DOCKER_IMG ?= "alange0001/rocksdb_test"
DOCKER_TAG ?= "latest"

all: opt docker

opt: rocksdb rocksdb_test ycsb

rocksdb-build:
	@echo "Compile rocksdb from $(ROCKSDB_SRC)"
	@cd "$(ROCKSDB_SRC)" && make DEBUG_LEVEL=0 -j6 static_lib shared_lib tools db_bench rocksdbjava

rocksdb:
	@echo "Install rocksdb from $(ROCKSDB_SRC) to $(ROCKSDB_DST)"
	@mkdir -p "$(ROCKSDB_DST)"/bin || true
	@cd "$(ROCKSDB_SRC)" && make DEBUG_LEVEL=0 PREFIX="$(ROCKSDB_DST)" install
	@find "$(ROCKSDB_SRC)" -maxdepth 1 -type f -perm /u=x -exec cp {} "$(ROCKSDB_DST)/bin" \;
	@find "$(ROCKSDB_SRC)/tools" -maxdepth 1 -type f -perm /u=x -exec cp {} "$(ROCKSDB_DST)/bin" \;
	@find "$(ROCKSDB_DST)/bin" -maxdepth 1 -type f -name 'lib*' -delete
	@find "$(ROCKSDB_SRC)" -maxdepth 1 -type f -name 'librocksdb_*.so' -exec cp {} "$(ROCKSDB_DST)/lib" \;
	@rm -f "$(ROCKSDB_DST)/lib/librocksdb.a"

rocksdb_test:
	@echo "Install rocksdb_test from $(ROCKSDB_TEST_SRC) to $(ROCKSDB_TEST_DST)"
	@mkdir -p "$(ROCKSDB_TEST_DST)"/bin || true
	@mkdir -p "$(ROCKSDB_TEST_DST)"/files || true
	@find "$(ROCKSDB_TEST_SRC)/files" -maxdepth 1 -type f -name '*.options' -exec cp {} "$(ROCKSDB_TEST_DST)/files" \;
	@cp "$(ROCKSDB_TEST_SRC)/build/access_time3" "$(ROCKSDB_TEST_DST)/bin"

ycsb:
	@echo "Install YCSB from $(YCSB_SRC) to $(YCSB_DST)"
	@mkdir -p "$(YCSB_DST)"/bin || true
	@mkdir -p "$(YCSB_DST)"/workloads || true
	@rsync -ax "$(YCSB_SRC)/bin/" "$(YCSB_DST)/bin/"
	@rsync -ax "$(YCSB_SRC)/lib/" "$(YCSB_DST)/lib/"
	@rsync -ax "$(YCSB_SRC)/workloads/" "$(YCSB_DST)/workloads/"
	@rsync -ax "$(YCSB_SRC)/rocksdb-binding/" "$(YCSB_DST)/rocksdb-binding/"
	@cd "$(YCSB_DST)/rocksdb-binding/lib" && rm -f rocksdbjni-*.jar 
	@find "$(ROCKSDB_SRC)/java/target" -maxdepth 1 -type f -name 'rocksdbjni-*.jar' -exec cp {} "$(YCSB_DST)/rocksdb-binding/lib" \;
	@test -z `fgrep YCSB_SLEEP $(YCSB_DST)/bin/ycsb.sh` && sed -i '17 a YCSB_SLEEP=$${YCSB_SLEEP:-0}; sleep "$$YCSB_SLEEP"' "$(YCSB_DST)/bin/ycsb.sh"

docker: opt
	@echo "Creating docker image"
	@docker build -t "$(DOCKER_IMG):$(DOCKER_TAG)" --compress .

clean:
	rm -fr opt