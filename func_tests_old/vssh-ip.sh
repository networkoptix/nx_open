#!/bin/bash
boxIP="$1"
shift
if [ -z "$boxIP" ]; then
    echo No box address provided!
    exit 1
fi
dir=`dirname $0`
files="$dir/ssh.*.conf"
for conf in $files; do
    if grep -q "HostName $boxIP" $conf; then
        box=`echo $conf | awk -F . '{print $(NF-1)}'`
        exec "$dir/vssh.sh" $box "$@"
    fi
done
