#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)

echo "Run server"
./server -c $SCRIPT_DIR/configs/server.cfg -l

