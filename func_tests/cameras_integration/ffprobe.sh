#!/bin/bash

ffprobe "$@" & $pid=$!
for i in 1..150; then
    if ps aux | awk '{print $2}' | grep $pid; then
        sleep 0.1
    else
        exit 0
    fi
done
kill -9 $pid
echo 'FFprobe was killed due to timeout' >&2
exit 1
