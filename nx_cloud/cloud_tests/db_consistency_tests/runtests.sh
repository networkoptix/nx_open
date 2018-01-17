#!/bin/sh

total_records=0

for f in *.sql
do
    echo -n "Test $f: "
    records=$(mysql -sN < $f | tee /dev/tty | wc -l)
    [ $? -ne 0 ] && exit 1
    echo "$records failures"

    total_records=$((total_records+records))
done

echo "Total: $total_records failures"

[ $total_records -eq 0 ]
