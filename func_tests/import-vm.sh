#!/bin/bash
set -euxo pipefail
: ${1:?"First parameter is existing VM name without '-template'."}
: ${2:?"Second parameter is dir to put exported VM to."}
NAME="$1-template"
EXPORTED="$2/$NAME.ova"
VBoxManage modifyvm "$NAME" --name "$NAME backup" || >&2 echo "OK if not exists."
VBoxManage import "$EXPORTED"  --vsys 0 --vmname "$NAME"
VBoxManage snapshot "$NAME" take "template"
VBoxManage unregistervm "$NAME backup" --delete || >&2 echo "OK if not exists."
