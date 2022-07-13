#!/bin/bash
check()
{
NUMBER=$(ps -eo 'tty,pid,comm' | grep ^? | grep client | awk '{print $2}' | wc -l)
}

check
echo "Number of live clients to kill: $NUMBER"

if [[ "$NUMBER" > 0 ]]; then
    kill -9 $(ps -eo 'tty,pid,comm' | grep ^? | grep client | awk '{print $2}')
    check
    echo "Now live clients: $NUMBER"
fi

