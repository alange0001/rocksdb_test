#!/bin/bash
ulimit -n 1048576

function test_dir() {
	if [ ! -d "$1" ]; then
		echo "Directory not found: $1" >&2
		exit 1
	fi
}

DIR_BASE=$(cd `dirname $0` && echo $PWD)
[ -z "$DIR_BASE" ] && exit 1
DIR_YCSB="$DIR_BASE/../YCSB"
DIR_ROCKS="$DIR_BASE/../rocksdb"

test_dir "$DIR_BASE/build"
export PATH="$PATH:$DIR_BASE/build"
test_dir "$DIR_YCSB/bin"
export PATH="$PATH:$DIR_YCSB/bin"
test_dir "$DIR_ROCKS"
export PATH="$PATH:$DIR_ROCKS"

DIR_WORK="/media/auto/work"
DIR_DB_BENCH="$DIR_WORK/rocksdb"
DIR_DB_YCSB="$DIR_WORK/rocksdb_ycsb"
DIR_AT3="$DIR_WORK/tmp"
test_dir "$DIR_AT3"



function run_at3() {
	output_dir="$1"
	if [ -z "$output_dir" ]; then
		echo "output_dir not defined"
		return
	fi
	
	t=1
	
	at_script="${t}:wait=false"
	for wr in 0 0.1 0.2 0.3 0.5 0.7 1; do
		at_script="$at_script,${t}:write_ratio=${wr}"
		for rr in 0 0.5 1; do
			at_script="$at_script,${t}:random_ratio=${rr}"
			for bs in 4 8 128 256 512; do
				at_script="$at_script,${t}:block_size=${bs}"
				t=$((t + 35))
			done
		done
	done
	at_script="$at_script,$((t + 5)):stop"
	for n in 1 6 2 3 4 5; do
		at_files="$(for ((i=0; i<$n; i++)); do echo "$DIR_AT3/$i"; done |paste -sd';')"
		echo $at_files
		echo $at_script
		rocksdb_test \
			--log_level="info" \
			--duration="$((t / 60 + 1))" \
			--warm_period="0" \
			--stats_interval="5" \
			--io_device="nvme0n1" \
			--num_at="$n" \
			--at_file="$at_files" \
			--at_params="--wait --flush_blocks=0" \
			--at_script="$at_script" >"$output_dir/at_files-$n.out"
	done
}

function run_at3_rww() {
	output_dir="$1"
	if [ -z "$output_dir" ]; then
		echo "output_dir not defined"
		return
	fi
	
	n=4
	t=1
	
	at_script="${t}:wait=false"
	t=$((t + 30))
	for ((i=1; i<$n; i++)); do
		at_script="$at_script;${t}:wait=false"
		for wr in 0 0.05 0.1 0.2 0.3 0.5 0.6 0.7 0.8 0.9 1; do
			at_script="$at_script,${t}:write_ratio=${wr}"
			t=$((t + 20))
		done
	done
	at_script="$at_script,$((t + 20)):stop"
	
	at_files="$(for ((i=0; i<$n; i++)); do echo "$DIR_AT3/$i"; done |paste -sd';')"
	echo $at_files
	echo $at_script
	for bs in 4 8 128 256 512; do
		rocksdb_test \
			--log_level="info" \
			--duration="$((t / 60 + 10))" \
			--warm_period="0" \
			--stats_interval="5" \
			--io_device="nvme0n1" \
			--num_at="$n" \
			--at_file="$at_files" \
			--at_params="--wait --direct_io --block_size=${bs}" \
			--at_script="$at_script" >"$output_dir/at3_rww_files${n}_bs${bs}.out"
	done
}

function run_ycsb() {
	test_dir "$DIR_DB_YCSB"
	ydb_threads=5
	num_at=6
	at_files="$(for ((i=0; i<$num_at; i++)); do echo "$DIR_AT3/$i"; done |paste -sd';')"
	at_script="$(for ((i=0; i<$num_at; i++)); do t=$((20 + i)); echo -n ${t}m:wait=false; for j in 0.1 0.2 0.3 0.5 0.7 1; do t=$((t + 6)); echo -n ,${t}m:write_ratio=$j; done; echo; done |paste -s -d';')"
	at_block_size=4
	
	rocksdb_test \
		--log_level="info" \
		--duration="70" \
		--warm_period="10" \
		--stats_interval="5" \
		--io_device="nvme0n1" \
		--num_ydbs="1" \
		--rocksdb_config_file="$DIR_BASE/files/rocksdb.db_bench.options" \
		--ydb_create="false" \
		--ydb_path="$DIR_DB_YCSB" \
		--ydb_workload="$DIR_YCSB/workloads/workloada" \
		--ydb_num_keys="50000000" \
		--ydb_threads="$ydb_threads" \
		--num_at="$num_at" \
		--at_file="$at_files" \
		--at_block_size="$at_block_size" \
		--at_params="--direct_io --random_ratio=0.5 --wait" \
		--at_script="$at_script"
}

function run_db_bench() {
	test_dir "$DIR_DB_BENCH"
	db_threads=9
	num_at=6
	at_files="$(for ((i=0; i<$num_at; i++)); do echo "$DIR_AT3/$i"; done |paste -sd';')"
	at_script="$(for ((i=0; i<$num_at; i++)); do t=$((20 + i)); echo -n ${t}m:wait=false; for j in 0.1 0.2 0.3 0.5 0.7 1; do t=$((t + 6)); echo -n ,${t}m:write_ratio=$j; done; echo; done |paste -s -d';')"
	at_block_size=512
	
	rocksdb_test \
		--log_level="info" \
		--duration="70" \
		--warm_period="10" \
		--stats_interval="5" \
		--io_device="nvme0n1" \
		--num_dbs="1" \
		--db_create="false" \
		--db_benchmark="readwhilewriting" \
		--db_path="$DIR_DB_BENCH" \
		--db_num_keys="500000000" \
		--db_cache_size="268435456" \
		--db_threads="$db_threads" \
		--num_at="$num_at" \
		--at_file="$at_files" \
		--at_block_size="$at_block_size" \
		--at_params="--direct_io --random_ratio=0.5 --wait" \
		--at_script="$at_script"
}

case "$1" in
	"db_bench")
		run_db_bench
		;;
	"ycsb")
		run_ycsb
		;;
	"at3")
		run_at3 "$2"
		;;
	"at3_rww")
		run_at3_rww "$2"
		;;
	*)
		echo "invalid action: '$1'"
		;;
esac
