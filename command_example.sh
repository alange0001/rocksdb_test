
db_threads=9
ydb_threads=5
ydb_workload_path="$HOME/workspace/dr/YCSB/workloads"

num_at=6
at_files="$(for ((i=0; i<$num_at; i++)); do echo /media/auto/work/tmp/$i; done |paste -sd';')"
#at_script="$(for i in 20 25 30 35 40 45; do echo ${i}m:wait=false; done |paste -sd';')"
#at_script="$(for ((i=0; i<6; i++)); do t=$((20 + 2*i)); echo -n ${t}m:wait=false; for j in 0.25 0.5 0.75 1; do t=$((t + 12)); echo -n ,${t}m:write_ratio=$j; done; echo; done |paste -s -d';')"
at_block_size=512

case "$1" in
	"run")
		rocksdb_test \
			--log_level="info" \
			--duration="55" \
			--warm_period="10" \
			--stats_interval="5" \
			--io_device="nvme0n1" \
			--num_dbs="1" \
			--db_create="false" \
			--db_benchmark="readwhilewriting" \
			--db_path="/media/auto/work/rocksdb" \
			--db_num_keys="500000000" \
			--db_cache_size="268435456" \
			--db_threads="$db_threads" \
			--num_at="$num_at" \
			--at_file="$at_files" \
			--at_block_size="$at_block_size" \
			--at_params="--direct_io --random_ratio=0.6 --write_ratio=0.3 --wait" \
			--at_script="$at_script"
		;;
	"runycsb")
		rocksdb_test \
			--log_level="info" \
			--duration="70" \
			--warm_period="10" \
			--stats_interval="5" \
			--io_device="nvme0n1" \
			--num_ydbs="1" \
			--rocksdb_config_file="$HOME/workspace/dr/rocksdb_test/files/rocksdb.db_bench.options" \
			--ydb_create="false" \
			--ydb_path="/media/auto/work/rocksdb_ycsb" \
			--ydb_workload="$ydb_workload_path/workloada" \
			--ydb_num_keys="50000000" \
			--ydb_threads="$ydb_threads"
			#--num_at="$num_at" \
			#--at_file="$at_files" \
			#--at_block_size="$at_block_size" \
			#--at_params="--direct_io --random_ratio=0.6 --write_ratio=0 --wait" \
			#--at_script="$at_script"
		;;
	"runat")
		rocksdb_test \
			--log_level="info" \
			--duration="31" \
			--warm_period="1" \
			--stats_interval="5" \
			--io_device="nvme0n1" \
			--num_at="$num_at" \
			--at_file="$at_files" \
			--at_block_size="$at_block_size" \
			--at_params="--direct_io --random_ratio=0.6 --write_ratio=0.3 --wait" \
			--at_script="$at_script"
		;;
	*)
		echo "no action"
		;;
esac
