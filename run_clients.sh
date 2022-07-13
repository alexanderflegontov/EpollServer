#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)

NUM_CLIENTS=1
if [[ "$@" > 1 ]]; then
    NUM_CLIENTS=$1
fi
echo "run $NUM_CLIENTS number of clients"

for ((n=0;n<$NUM_CLIENTS;n++))
do
    echo "run clint# $n"
    ./client -c $SCRIPT_DIR/configs/client.cfg -l
done

echo "done!"

