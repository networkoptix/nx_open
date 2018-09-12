#!/bin/bash

ffprobe "$@" & pid=$!
for i in `seq 1 150`; do
    if ps aux | awk '{print $2}' | grep $pid >/dev/null 2>&1; then
        sleep 0.1
    else
        exit 0
    fi
done
kill -9 $pid
echo 'FFprobe was killed due to timeout' >&2
exit 1
