#!/bin/bash

## Done:
#RESTORE_DB=1 AT_BLOCK_SIZE=512 AT_DIRECT_IO=0 ./run.sh db_bench >plot/dbbench_wwr,at3_bs512_cache.out
#RESTORE_DB=1 AT_BLOCK_SIZE=4   AT_DIRECT_IO=0 ./run.sh db_bench >plot/dbbench_wwr,at3_bs4_cache.out

#RESTORE_DB=1 WORKLOAD=workloada NUM_AT=0 ./run.sh ycsb >plot/ycsb_wa.out
#RESTORE_DB=1 WORKLOAD=workloadb NUM_AT=0 ./run.sh ycsb >plot/ycsb_wb.out

#RESTORE_DB=1 AT_BLOCK_SIZE=4   AT_DIRECT_IO=0 WORKLOAD=workloada ./run.sh ycsb >plot/ycsb_wa,at3_bs4_cache.out
#RESTORE_DB=1 AT_BLOCK_SIZE=4   AT_DIRECT_IO=0 WORKLOAD=workloadb ./run.sh ycsb >plot/ycsb_wb,at3_bs4_cache.out
#RESTORE_DB=1 AT_BLOCK_SIZE=512 AT_DIRECT_IO=0 WORKLOAD=workloada ./run.sh ycsb >plot/ycsb_wa,at3_bs512_cache.out
#RESTORE_DB=1 AT_BLOCK_SIZE=512 AT_DIRECT_IO=0 WORKLOAD=workloadb ./run.sh ycsb >plot/ycsb_wb,at3_bs512_cache.out

#RESTORE_DB=1 AT_BLOCK_SIZE=4   AT_DIRECT_IO=1 WORKLOAD=workloada ./run.sh ycsb >plot/ycsb_wa,at3_bs4_directio.out
#RESTORE_DB=1 AT_BLOCK_SIZE=4   AT_DIRECT_IO=1 WORKLOAD=workloadb ./run.sh ycsb >plot/ycsb_wb,at3_bs4_directio.out
#RESTORE_DB=1 AT_BLOCK_SIZE=512 AT_DIRECT_IO=1 WORKLOAD=workloada ./run.sh ycsb >plot/ycsb_wa,at3_bs512_directio.out
#RESTORE_DB=1 AT_BLOCK_SIZE=512 AT_DIRECT_IO=1 WORKLOAD=workloadb ./run.sh ycsb >plot/ycsb_wb,at3_bs512_directio.out


## Rebuild:
# 




## exp_db_nthreads:
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloadb THREADS=4 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wb_t4.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloadb THREADS=5 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wb_t5.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloadb THREADS=6 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wb_t6.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloadb THREADS=7 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wb_t7.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloadb THREADS=8 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wb_t8.out

#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloada THREADS=4 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wa_t4.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloada THREADS=5 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wa_t5.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloada THREADS=6 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wa_t6.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloada THREADS=7 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wa_t7.out
#DURATION=15 WARM_PERIOD=1 RESTORE_DB=0 WORKLOAD=workloada THREADS=8 NUM_AT=0 ./run.sh ycsb >plot/ycsb_wa_t8.out
