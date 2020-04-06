#!/bin/bash

function get_glibcxx_version
{
    local -r FILE="$1"
    # `sed` searches for something like `blabla@@GLIBCXX_3.4.25` and prints `3.4.25`.
    readelf -sV "$FILE" | sed -n 's/.*@@GLIBCXX_//p' | sort -uV | tail -n 1
}

LOCAL_STDCPP_DIR="$1"
if [ -z "$LOCAL_STDCPP_DIR" ]
then
    echo "Please specify local libstdc++ directory." >&2
    exit 1
fi

LOCAL_STDCPP="$LOCAL_STDCPP_DIR/libstdc++.so.6"
if [ ! -f "$LOCAL_STDCPP" ]
then
    echo "File not found: $LOCAL_STDCPP" >&2
    exit 1
fi

SYSTEM_STDCPP="$(ldconfig -p | grep libstdc++.so.6 | head -n 1 | sed 's/.*=> //')"
if [ -z "$SYSTEM_STDCPP" ]
then
    echo "$LOCAL_STDCPP"
    exit 0
fi

LOCAL_VERSION=$(get_glibcxx_version "$LOCAL_STDCPP")
SYSTEM_VERSION=$(get_glibcxx_version "$SYSTEM_STDCPP")

MAX_VERSION=$((echo $LOCAL_VERSION; echo $SYSTEM_VERSION) | sort -V | tail -n 1)

if [[ $LOCAL_VERSION == $MAX_VERSION ]]
then
    echo "$LOCAL_STDCPP_DIR"
fi
