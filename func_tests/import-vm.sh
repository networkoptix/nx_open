#!/bin/bash
set -euxo pipefail
: ${1:?"First parameter is existing exported VM file name without '-template.ova'."}
: ${2:?"Second parameter is dir to get exported VM from."}
NAME="$1-template"
EXPORTED="$2/$NAME.ova"
VBoxManage modifyvm "$NAME" --name "$NAME-backup" || >&2 echo "OK if not exists."
VBoxManage import "$EXPORTED"  --vsys 0 --vmname "$NAME"
VBoxManage snapshot "$NAME" take "template"
VBoxManage unregistervm "$NAME-backup" --delete || >&2 echo "OK if not exists."
