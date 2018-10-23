#!/bin/bash

set -euxo pipefail

# Used variables.
: ${BASE_DIR:?"Run: BASE_DIR=... ./bootstrap.sh; func_tests-bin, func_tests-work, func_tests-venv will appear there."}
: ${BIN_DIR:="${BASE_DIR}/func_tests-bin"}
: ${WORK_DIR:="${BASE_DIR}/func_tests-work"}
: ${VENV_DIR:="${BASE_DIR}/func_tests-venv"}
: ${VENV3_DIR:="${BASE_DIR}/func_tests-venv3"}

# Quick checks.
mkdir -p "${WORK_DIR}"
mkdir -p "${BIN_DIR}"
type pytest && exit 4  # `pytest` MUST NOT be installed globally.

# See: https://wiki.debian.org/VirtualBox#Debian_9_.22Stretch.22
CODENAME=$(lsb_release --codename --short)
sudo apt-get update
sudo apt-get install --yes \
    python-virtualenv python2.7-dev \
    python3-venv python3-dev \
    virtualbox virtualbox-ext-pack \
    rsync \
    ffmpeg \
    smbclient
sudo wget -O /etc/bash_completion.d/VBoxManage https://raw.githubusercontent.com/gryf/vboxmanage-bash-completion/d4f56a0d6b24ab8585dc1a3c86dd56f4235fe106/VBoxManage
sudo chmod +x /etc/bash_completion.d/VBoxManage
echo "Reload Bash to enable VBoxManage completion:"
echo ". ~/.bashrc"

rm -fr "${VENV_DIR}"
python2.7 -m virtualenv "${VENV_DIR}"
"${VENV_DIR}/bin/pip" install -U pip setuptools wheel  # Update to avoid compilation where possible.
"${VENV_DIR}/bin/pip" install -r requirements.txt
rsync -av rsync://noptix.enk.me/buildenv/test/ "${BIN_DIR}/"

rm -fr "${VENV3_DIR}"
python3 -m venv "${VENV3_DIR}"
"${VENV3_DIR}/bin/pip" install -U pip setuptools wheel  # Update to avoid compilation where possible.
"${VENV3_DIR}/bin/pip" install -r requirements3.txt
rsync -av rsync://noptix.enk.me/buildenv/test/ "${BIN_DIR}/"

echo Check if basics work with "${VENV_DIR}/bin/pytest" --collect-only --mediaserver-installers-dir=~/Downloads:
"${VENV_DIR}/bin/pytest" --collect-only --mediaserver-installers-dir=~/Downloads

echo Download mediaserver deb package and specify path to it with --mediaserver-dist-path= option.
echo Run tests:
echo "${VENV_DIR}/bin/pytest" -s --work-dir="${BASE_DIR}" --bin-dir="${BIN_DIR}" --mediaserver-installers-dir=~/Downloads
