#!/bin/bash

set -euxo pipefail

# Used variables.
: ${BASE_DIR:?}
: ${BIN_DIR:="${BASE_DIR}/func_tests-bin"}
: ${WORK_DIR:="${BASE_DIR}/func_tests-work"}
: ${VENV_DIR:="${BASE_DIR}/func_tests-venv"}

# Quick checks.
mkdir -p "${WORK_DIR}"
mkdir -p "${BIN_DIR}"
type pytest && exit 4  # `pytest` MUST NOT be installed globally.

sudo apt-get update
sudo apt-get install software-properties-common
sudo apt-add-repository http://download.virtualbox.org/virtualbox/debian
wget https://www.virtualbox.org/download/oracle_vbox_2016.asc -O- | sudo apt-key add -
wget https://www.virtualbox.org/download/oracle_vbox.asc -O- | sudo apt-key add -
sudo apt-get update

test -e "${VENV_DIR}" || virtualenv --system-site-packages "${VENV_DIR}"
"${VENV_DIR}/bin/pip" install -U pip setuptools wheel  # Update to avoid compilation where possible.
"${VENV_DIR}/bin/pip" install -r requirements.txt
rsync -av rsync://noptix.enk.me/buildenv/test/ "${BIN_DIR}/"

echo Check if basics work with "${VENV_DIR}/bin/pytest" --collect-only --mediaserver-installers-dir=~/Downloads:
"${VENV_DIR}/bin/pytest" --collect-only --mediaserver-installers-dir=~/Downloads

echo Download mediaserver deb package and specify path to it with --mediaserver-dist-path= option.
echo Run tests:
echo "${VENV_DIR}/bin/pytest" -s --work-dir="${BASE_DIR}" --bin-dir="${BIN_DIR}" --mediaserver-installers-dir=~/Downloads
