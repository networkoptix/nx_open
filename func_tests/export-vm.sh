#!/bin/bash
set -euxo pipefail
: ${1:?"First parameter is existing VM name."}
: ${2:?"Second parameter is dir to put exported VM to."}
NAME="$1"
EXPORTED="$2/$NAME.ova"
TEMP="$2/$NAME.bu.ova"
mkdir -p "$(dirname "$TEMP")"
VBoxManage export "$NAME" -o "$TEMP" --options nomacs
mkdir -p "$(dirname "$EXPORTED")"
mv "$TEMP" "$EXPORTED"
