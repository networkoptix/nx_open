#!/bin/bash
set -euxo pipefail
: ${1:?"First parameter is existing VM name without '-template'."}
: ${2:?"Second parameter is dir to put exported VM to."}
NAME="$1-template"
EXPORTED="$2/$NAME.ova"
mkdir -p "$(dirname "$EXPORTED")"
VBoxManage export "$NAME" -o "$EXPORTED" --options nomacs
