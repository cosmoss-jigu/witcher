#!/bin/bash

REDIS_DIR=$WITCHER_HOME/benchmark/redis-3.2-nvml
MAIN_DIR=$REDIS_DIR/main
LIB_DIR=$REDIS_DIR/lib

PM_FILE_PATH=$1
PM_SIZE=$2mb
PM_LAYOUT=$3
OP_FILE_PATH=$4
OP_INDEX=$5
SKIP_INDEX=$6
OUTPUT_PATH=$7
MEM_LAYOUT_PATH=$8

echo "starting redis"
$MAIN_DIR/main.exe $LIB_DIR/redis.conf pmfile $PM_FILE_PATH $PM_SIZE > /dev/null &
sleep 5

echo "starting client"
$MAIN_DIR/client.sh $OP_INDEX $SKIP_INDEX $OP_FILE_PATH | telnet localhost 6379 | grep -v "Trying" | grep -v "Connected" | grep -v "Escape" > $OUTPUT_PATH
echo "closing server"
