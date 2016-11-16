#!/bin/bash

set -e

ROOT="$environment/packages"

function upload() {
    PACKAGE=$1
    PACKAGE_DIR="$ROOT/any/$PACKAGE"

    [ -d "$PACKAGE_DIR" ] && rm -rf "$PACKAGE_DIR"

    mkdir -p "$PACKAGE_DIR"/bin
    cp -f ${basedir}/external.dat "$PACKAGE_DIR"/bin
    ${root.dir}/build_utils/python/rdep.py -u -f -t any $PACKAGE --root $ROOT
}

upload "server-external-${branch}"
[[ "${branch}" == "prod_${release.version}" ]] && upload "server-external-${release.version}"
