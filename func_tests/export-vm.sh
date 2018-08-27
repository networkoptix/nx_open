#!/bin/bash
set -euxo pipefail
: ${1:?"First parameter is existing VM name."}
: ${2:?"Second parameter is path to new VM image."}
NAME="$1"
EXPORTED="$2"
mkdir -p "$(dirname "$EXPORTED")"
VBoxManage export "$NAME" -o "$EXPORTED" --options nomacs
