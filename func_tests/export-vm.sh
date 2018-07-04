#!/bin/bash
set -euxo pipefail
: ${1:?"First parameter is existing VM name."}
: ${2:?"Second parameter is path to new VM image."}
NAME="$1"
EXPORTED="$2"
TEMP="$2.bu"
mkdir -p "$(dirname "$TEMP")"
VBoxManage export "$NAME" -o "$TEMP" --options nomacs
mkdir -p "$(dirname "$EXPORTED")"
mv "$TEMP" "$EXPORTED"
