#!/bin/bash
set -e

function exitWithError
{
    echo >&2 "$1"
    exit 1
}

CONFIG_FILE="$1"
KEY="$2"
VALUE="$3"

[ -z "$CONFIG_FILE" ] && exitWithError "Config file is not specified."
[ -z "$KEY" ] && exitWithError "Key is not specified."

if [ -z "$VALUE" ]
then
    # Read value.
    [ ! -f "$CONFIG_FILE" ] && exit 0
    grep -E "^$KEY\\s*=" "$CONFIG_FILE" | head -n1 | sed 's/.*=\s*\(.*\)\s*/\1/'
elif [ "$VALUE" == "-d" ] || [ "$VALUE" == "--delete" ]
then
    # Delete value.
    [ ! -f "$CONFIG_FILE" ] && exit 0
    sed -i "/^$KEY\\s*=/d" "$CONFIG_FILE"
else
    if [ ! -f "$CONFIG_FILE" ]
    then
        echo "[General]" > "$CONFIG_FILE"
        echo "$KEY=$VALUE" >> "$CONFIG_FILE"
    elif grep -qE "^$KEY\\s*=" "$CONFIG_FILE"
    then
        # Replace value.
        sed -i "s/^$KEY\\s*=.*/$KEY=$VALUE/" "$CONFIG_FILE"
    else
        # Insert value.
        sed -i "/\\[General\\]/Ia $KEY=$VALUE" "$CONFIG_FILE"
    fi
fi
