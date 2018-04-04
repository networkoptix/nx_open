#!/bin/bash
set -euxo pipefail
ssh "$2" test -e ~/".func_tests/$1-template.ova" || ssh "$2" VBoxManage export "$1-template" -o ~/".func_tests/$1-template.ova"
mkdir -p ".func_tests"
rsync --progress --stats "$2:.func_tests/$1-template.ova" ~/".func_tests/$1-template.ova"
rsync --progress --stats "$2:.func_tests/$1-key.key" ~/".func_tests/$1-key.key"
VBoxManage import ".func_tests/$1-template.ova"
VBoxManage snapshot "$1-template" take "template"
