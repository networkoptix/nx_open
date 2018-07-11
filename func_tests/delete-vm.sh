#!/bin/bash
set -u
: ${1:?"First parameter is regex; VMs, names of which match, will be deleted."}
VBoxManage list runningvms | cut -d'"' -f 2 | grep -E "$1" | xargs -I {} VBoxManage controlvm {} poweroff
VBoxManage list vms | cut -d'"' -f 2 | grep -E "$1" | xargs -I {} VBoxManage unregistervm {} --delete
