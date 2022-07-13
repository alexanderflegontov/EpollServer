#!/bin/bash

# Here is the path to boost shared libraries:
export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH

# In order to work successfully when launching with logging and default configs:
sudo mkdir -p /var/log/generator/

