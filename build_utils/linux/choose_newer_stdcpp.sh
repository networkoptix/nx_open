#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function get_glibcxx_version
{
    local -r FILE="$1"

    # `nm` allows us to certainly know the library version, so trying it first.
    if command -v nm > /dev/null
    then
        # `sed` searches for something like `0000000000000000 A GLIBCXX_3.4.25` and prints `3.4.25`.
        nm -D "$FILE" | sed -n 's/.* GLIBCXX_//p' | sort -uV | tail -n 1
    else
        # If `nm` is not present, guess the version by the library file name.
        # `sed` cuts everything but version part.
        realpath "$FILE" | sed 's/.*\.so\.//'
    fi
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
