#!/bin/bash
set -euxo pipefail
test -e ~/".func_tests/$1-template.ova" || VBoxManage export "$1-template" -o ~/".func_tests/$1-template.ova"
ssh "$2" mkdir -p ".func_tests"
rsync --progress --stats ~/".func_tests/$1-template.ova" "$2:.func_tests/$1-template.ova"
rsync --progress --stats ~/".func_tests/$1-key.key" "$2:.func_tests/$1-key.key"
ssh "$2" VBoxManage import ".func_tests/$1-template.ova"
ssh "$2" VBoxManage snapshot "$1-template" take "template"
