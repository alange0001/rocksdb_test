#!/bin/bash

set -e

output_prefix="00_"
args_file="/tmp/rocksdb_test_helper.args"
config_file="/tmp/rocksdb_config.options"

function main() {

	if [ "$1" == "01" ]; then
		output_prefix="${1}_"
		save_args
		save_config01

		./rocksdb_test_helper --load_args="$args_file" create_ycsb
		sleep 1m
		./rocksdb_test_helper --load_args="$args_file" ycsb
	fi
	
	if [ "$1" == "02" ]; then
		output_prefix="${1}_"
		save_args
		save_config02

		./rocksdb_test_helper --load_args="$args_file" create_ycsb
		sleep 1m
		./rocksdb_test_helper --load_args="$args_file" ycsb
	fi

	if [ "$1" == "03" ]; then
		output_prefix="${1}_"
		save_args
		level0_file_num_compaction_trigger=2 save_config02

		./rocksdb_test_helper --load_args="$args_file" create_ycsb
		sleep 1m
		./rocksdb_test_helper --load_args="$args_file" ycsb
	fi

	if [ "$1" == "04" ]; then
		output_prefix="${1}_"
		save_args
		level0_file_num_compaction_trigger=2 min_write_buffer_number_to_merge=2 save_config02

		./rocksdb_test_helper --load_args="$args_file" create_ycsb
		sleep 1m
		./rocksdb_test_helper --load_args="$args_file" ycsb
	fi

	if [ "$1" == "05" ]; then
		output_prefix="${1}_"
		save_args
		level0_file_num_compaction_trigger=2 min_write_buffer_number_to_merge=2 num_levels=5 save_config02

		./rocksdb_test_helper --load_args="$args_file" create_ycsb
		sleep 1m
		./rocksdb_test_helper --load_args="$args_file" ycsb
	fi

	if [ "$1" == "06" ]; then
		output_prefix="${1}_"
		save_args
		save_config03

		if [ -z "$2" ] || [ "$2" == 'a' ]; then
			./rocksdb_test_helper --load_args="$args_file" create_ycsb --duration=20
			./rocksdb_test_helper --load_args="$args_file" ycsb --ydb_workload_list="workloada workloadb"
			
			tar -cf /media/auto/work2/rocksdb_6.15_tune/ycsb_db3tune_${1}.tar -C /media/auto/s980pro250/rocksdb_ycsb_0 .
			
			[ "$2" == 'a' ] && shift 1
		fi
		
		if [ "$2" == 'b' ]; then
			duration=75 warm_period=15 save_args
			
			./rocksdb_test_helper --load_args="$args_file" ycsb_at3 --ydb_workload_list="workloada workloadb"
			
			shift 1
		fi

		if [ "$2" == 'c' ]; then
			duration=75 warm_period=15 save_args
			
			./rocksdb_test_helper --load_args="$args_file" ycsb_at3 \
				--ydb_workload_list="workloada workloadb" \
				--at_block_size_list="256 512"
			
			shift 1
		fi

		shift 1
	fi

	if [ "$1" == "06.s970evo500" ]; then
		output_prefix="${1}_"
		data_path="/media/auto/s970evo500"
		save_args
		save_config03

		./rocksdb_test_helper --load_args="$args_file" ycsb \
			--backup_ycsb="/media/auto/work2/rocksdb_6.15_tune/ycsb_db3tune_06.tar" \
			--ydb_workload_list="workloada"
		./rocksdb_test_helper --load_args="$args_file" ycsb \
			--ydb_workload_list="workloadb"

		shift 1
	fi

	if [ "$1" == "07" ]; then
		exp_n="$1"
		output_prefix="${exp_n}${exp_pref}-"
		ydb_workload_list=${ydb_workload_list:-workloada workloadb}
		save_config04

		if [ "$2" == 'create' ]; then
			duration=${duration:-70}        \
			save_args
			./rocksdb_test_helper --load_args="$args_file" create_ycsb --duration=20
			sleep 1m
			./rocksdb_test_helper --load_args="$args_file" ycsb
			
			shift 1
		fi
		
		if [ "$2" == 'backup' ]; then
			save_args
			cp "`dirname ${BASH_SOURCE[0]}`/${exp_n}_rocksdb.options" /media/auto/work2/rocksdb_6.15_tune/ycsb_db3tune_${exp_n}.options
			tar -cf /media/auto/work2/rocksdb_6.15_tune/ycsb_db3tune_${exp_n}.tar -C "/media/auto/s980pro250/rocksdb_ycsb_0" .
			shift 1
		fi

		if [ "$2" == 'steady' ]; then
			duration=${duration:-70}        \
			save_args
			./rocksdb_test_helper --load_args="$args_file" ycsb
			shift 1
		fi

		if [ "$2" == 'pressure' ]; then
			duration=75 warm_period=15 save_args
			
			./rocksdb_test_helper --load_args="$args_file" ycsb_at3
			
			shift 1
		fi

		if [ "$2" == 'pressure2' ]; then
			duration=${duration:-70}        \
			warm_period=${warm_period:-10}  \
			at_interval=${at_interval:-5}   \
			at_script_gen=2                 \
			save_args
			
			./rocksdb_test_helper --load_args="$args_file" ycsb_at3
			
			shift 1
		fi

		if [ "$2" == 'pressure3' ]; then
			warm_period=${warm_period:-30}  \
			at_interval=${at_interval:-2}   \
			at_script_gen=3                 \
			duration=${duration:--1}        \
			save_args
			
			./rocksdb_test_helper --load_args="$args_file" ycsb_at3
			
			shift 1
		fi

		shift 1
	fi

}

###########################################################################################
function save_args() {
local data_path=${data_path:-/media/auto/s980pro250}
local duration=${duration:-90}
local warm_period=${warm_period:-10}
local ydb_workload_list=${ydb_workload_list:-workloadb}
local num_at=${num_at:-4}
local at_iodepth_list=${at_iodepth_list:-1}
local at_io_engine=${at_io_engine:-libaio}
local at_script_gen=${at_script_gen:-1}
local at_interval=${at_interval:-2}
local at_block_size_list=${at_block_size_list:-4 8 16 32 64 128 256 512}
local at_o_dsync=${at_o_dsync:-true}
local ydb_rocksdb_jni=${ydb_rocksdb_jni:-../rocksdb.dev/java/target/rocksdbjni-6.15.5-linux64.jar}

cat <<EOB >"$args_file"
{
	"output_path": "`dirname ${BASH_SOURCE[0]}`",
	"output_prefix": "$output_prefix",
	"data_path": "$data_path",
	"before_run_cmd": "echo telegram-send 'Initiating experiment {output}'; sudo fstrim $data_path; sleep 30",
	"after_run_cmd": "telegram-send 'Experiment {output} returned exit code {exit_code}'",
	"rocksdb_config_file": "$config_file",
	"__backup_ycsb": "$backup_file",
	"duration": $duration,
	"warm_period": $warm_period,
	"ydb_workload_list": "$ydb_workload_list",
	"ydb_num_keys": 50000000,
	"ydb_threads": 5,
	"num_at": $num_at,
	"at_iodepth_list": "$at_iodepth_list",
	"at_io_engine": "$at_io_engine",
	"at_o_dsync": "$at_o_dsync",
	"at_script_gen": $at_script_gen,
	"at_interval": $at_interval,
	"at_block_size_list": "$at_block_size_list",
	"params": "--perfmon --ydb_rocksdb_jni=$ydb_rocksdb_jni --ydb_socket=true",
	"__docker_params": "-e ROCKSDB_TR_SOCKET=/tmp/host/rocksdb.sock"
}
EOB
}

###########################################################################################
function save_config01() {
cfs="${cfs:-default usertable}"

cat <<EOB2 > "$config_file"
[Version]
  rocksdb_version=6.15.5
  options_file_version=1.1

[DBOptions]
  bytes_per_sync=0
  create_if_missing=true
  create_missing_column_families=true
  db_write_buffer_size=0
  delayed_write_rate=$((16 * 1024 ** 2))
  enable_pipelined_write=false
  max_background_compactions=24
  max_background_flushes=-1
  max_background_jobs=24
  max_open_files=-1
  new_table_reader_for_compaction_inputs=false
  table_cache_numshardbits=6

EOB2

for c in $cfs; do
cat <<EOB2CF >> "$config_file"
[CFOptions "$c"]
  bottommost_compression=kDisableCompressionOption
  compaction_pri=kMinOverlappingRatio
  compaction_style=kCompactionStyleLevel
  compression=kSnappyCompression
  compression_per_level=kNoCompression:kNoCompression:kLZ4Compression:kLZ4Compression:kLZ4Compression:kLZ4Compression:kLZ4Compression
  force_consistency_checks=false
  hard_pending_compaction_bytes_limit=$((256 * 1024 ** 3))
  level0_file_num_compaction_trigger=2
  level0_slowdown_writes_trigger=20
  level0_stop_writes_trigger=36
  level_compaction_dynamic_level_bytes=false
  max_bytes_for_level_base=$((512 * 1024 ** 2))
  max_bytes_for_level_multiplier=10
  max_compaction_bytes=$((1 * 1024 ** 3))
  max_write_buffer_number=6
  min_write_buffer_number_to_merge=2
  num_levels=7
  table_factory=BlockBasedTable
  target_file_size_base=$((64 * 1024 ** 2))
  write_buffer_size=$((128 * 1024 ** 2))

[TableOptions/BlockBasedTable "$c"]
  block_cache=$((2 * 1024 ** 3))
  block_size=$((4 * 1024))
  cache_index_and_filter_blocks=false
  cache_index_and_filter_blocks_with_high_priority=true
  filter_policy=nullptr
  metadata_block_size=$((4 * 1024))
  no_block_cache=false
  optimize_filters_for_memory=true
  pin_l0_filter_and_index_blocks_in_cache=false

EOB2CF
done
cp -f "$config_file" "`dirname ${BASH_SOURCE[0]}`/${output_prefix}rocksdb.options"
}

###########################################################################################
function save_config02() {
cfs="${cfs:-default usertable}"
level0_file_num_compaction_trigger="${level0_file_num_compaction_trigger:-4}"
min_write_buffer_number_to_merge="${min_write_buffer_number_to_merge:-1}"
num_levels="${num_levels:-4}"
compression_per_level="${compression_per_level:-kNoCompression:kNoCompression:kLZ4Compression:kLZ4Compression:kLZ4Compression:kLZ4Compression:kLZ4Compression}"

cat <<EOB2 > "$config_file"
[Version]
  rocksdb_version=6.15.5
  options_file_version=1.1

[DBOptions]
  bytes_per_sync=8388608
  create_if_missing=true
  create_missing_column_families=true
  db_write_buffer_size=0
  delayed_write_rate=$((16 * 1024 ** 2))
  enable_pipelined_write=true
  max_background_compactions=16
  max_background_flushes=7
  max_background_jobs=24
  max_open_files=-1
  new_table_reader_for_compaction_inputs=true
  table_cache_numshardbits=4

EOB2

for c in $cfs; do
cat <<EOB2CF >> "$config_file"
[CFOptions "$c"]
  bottommost_compression=kLZ4Compression
  compaction_pri=kMinOverlappingRatio
  compaction_style=kCompactionStyleLevel
  compression=kLZ4Compression
  compression_per_level=$compression_per_level
  force_consistency_checks=false
  hard_pending_compaction_bytes_limit=$((256 * 1024 ** 3))
  level0_file_num_compaction_trigger=$level0_file_num_compaction_trigger
  level0_slowdown_writes_trigger=20
  level0_stop_writes_trigger=36
  level_compaction_dynamic_level_bytes=false
  max_bytes_for_level_base=$((512 * 1024 ** 2))
  max_bytes_for_level_multiplier=10
  max_compaction_bytes=$((3 * 1024 ** 3))
  max_write_buffer_number=6
  merge_operator=PutOperator
  min_write_buffer_number_to_merge=$min_write_buffer_number_to_merge
  num_levels=$num_levels
  table_factory=BlockBasedTable
  target_file_size_base=$((64 * 1024 ** 2))
  ttl=0
  write_buffer_size=$((128 * 1024 ** 2))

[TableOptions/BlockBasedTable "$c"]
  block_cache=$((2 * 1024 ** 3))
  block_size=$((8 * 1024))
  cache_index_and_filter_blocks=true
  cache_index_and_filter_blocks_with_high_priority=true
  filter_policy=bloomfilter:10:false
  metadata_block_size=$((4 * 1024))
  no_block_cache=false
  optimize_filters_for_memory=true
  pin_l0_filter_and_index_blocks_in_cache=true

EOB2CF
done
cp -f "$config_file" "`dirname ${BASH_SOURCE[0]}`/${output_prefix}rocksdb.options"
}

###########################################################################################
function save_config03() {
cfs="${cfs:-default usertable}"

cat <<EOB2 > "$config_file"
[Version]
  rocksdb_version=6.15.5
  options_file_version=1.1

[DBOptions]
  bytes_per_sync=$((8 * 1024**2))
  create_if_missing=true
  create_missing_column_families=true
  db_write_buffer_size=0
  delayed_write_rate=$((8 * 1024**2))
  enable_pipelined_write=true
  max_background_compactions=6
  max_background_flushes=6
  max_open_files=-1
  new_table_reader_for_compaction_inputs=true
  table_cache_numshardbits=4

EOB2

for c in $cfs; do
cat <<EOB2CF >> "$config_file"
[CFOptions "$c"]
  bottommost_compression=kLZ4Compression
  compaction_pri=kMinOverlappingRatio
  compaction_style=kCompactionStyleLevel
  compression=kLZ4Compression
  level0_file_num_compaction_trigger=4
  level0_slowdown_writes_trigger=20
  level_compaction_dynamic_level_bytes=false
  max_bytes_for_level_base=$((1 * 1024**3))
  max_bytes_for_level_multiplier=8
  max_write_buffer_number=8
  merge_operator=PutOperator
  min_write_buffer_number_to_merge=1
  num_levels=4
  target_file_size_base=$((128 * 1024**2))
  ttl=0
  write_buffer_size=$((128 * 1024**2))

[TableOptions/BlockBasedTable "$c"]
  block_cache=$((2 * 1024 ** 3))
  block_size=$((8 * 1024))
  cache_index_and_filter_blocks=true
  cache_index_and_filter_blocks_with_high_priority=true
  filter_policy=bloomfilter:10:false
  metadata_block_size=$((4 * 1024))
  no_block_cache=false
  optimize_filters_for_memory=true
  pin_l0_filter_and_index_blocks_in_cache=true

EOB2CF
done
cp -f "$config_file" "`dirname ${BASH_SOURCE[0]}`/${output_prefix}rocksdb.options"
}

###########################################################################################
function save_config04() {
cfs="${cfs:-default usertable}"
level0_file_num_compaction_trigger="${level0_file_num_compaction_trigger:-2}"
min_write_buffer_number_to_merge="${min_write_buffer_number_to_merge:-2}"
num_levels="${num_levels:-5}"
compression_per_level="${compression_per_level:-kNoCompression:kZSTD:kZSTD:kZSTD:kZSTD:kZSTD:kZSTD}"
compression_opts="${compression_opts:-{level=2}}"
bottommost_compression_opts="${bottommost_compression_opts:-{level=4}}"

cat <<EOB2 > "$config_file"
[Version]
  rocksdb_version=6.15.5
  options_file_version=1.1

[DBOptions]
  bytes_per_sync=$((8 * 1024 ** 2))
  create_if_missing=true
  create_missing_column_families=true
  db_write_buffer_size=0
  delayed_write_rate=$((16 * 1024 ** 2))
  enable_pipelined_write=true
  max_background_compactions=4
  max_background_flushes=7
  max_background_jobs=11
  max_open_files=-1
  new_table_reader_for_compaction_inputs=true
  table_cache_numshardbits=4

EOB2

for c in $cfs; do
cat <<EOB2CF >> "$config_file"
[CFOptions "$c"]
  compaction_pri=kMinOverlappingRatio
  compaction_style=kCompactionStyleLevel
  compression=kZSTD
  compression_per_level=$compression_per_level
  compression_opts=$compression_opts
  bottommost_compression_opts=$bottommost_compression_opts
  hard_pending_compaction_bytes_limit=$((256 * 1024 ** 3))
  level0_file_num_compaction_trigger=$level0_file_num_compaction_trigger
  level0_slowdown_writes_trigger=20
  level_compaction_dynamic_level_bytes=false
  max_bytes_for_level_base=$((512 * 1024 ** 2))
  max_bytes_for_level_multiplier=10
  max_compaction_bytes=$((3 * 1024 ** 3))
  max_write_buffer_number=6
  merge_operator=PutOperator
  min_write_buffer_number_to_merge=$min_write_buffer_number_to_merge
  num_levels=$num_levels
  target_file_size_base=$((64 * 1024 ** 2))
  ttl=0
  write_buffer_size=$((128 * 1024 ** 2))

[TableOptions/BlockBasedTable "$c"]
  block_cache=$((2 * 1024 ** 3))
  block_size=$((8 * 1024))
  cache_index_and_filter_blocks=true
  cache_index_and_filter_blocks_with_high_priority=true
  filter_policy=bloomfilter:10:false
  metadata_block_size=$((4 * 1024))
  no_block_cache=false
  optimize_filters_for_memory=true
  pin_l0_filter_and_index_blocks_in_cache=true

EOB2CF
done
cp -f "$config_file" "`dirname ${BASH_SOURCE[0]}`/${output_prefix}rocksdb.options"
}


###########################################################################################
main "$@"
