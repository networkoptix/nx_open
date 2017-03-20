#!/bin/bash

set -e

ROOT="$environment/packages"

function upload() {
    PACKAGE=$1
    PACKAGE_DIR="$ROOT/any/$PACKAGE"

    if [ -d "$PACKAGE_DIR" ]; then
        rm -rf "$PACKAGE_DIR"
    fi

    mkdir -p "$PACKAGE_DIR"/bin
    cp -f ${basedir}/external.dat "$PACKAGE_DIR"/bin
    cp -f ${basedir}/server-external.cmake "$PACKAGE_DIR"
    cp -f ${basedir}/qbs/server-external.qbs "$PACKAGE_DIR"
    ${root.dir}/build_utils/python/rdep.py -u -f -t any $PACKAGE --root $ROOT
}

upload "server-external-${branch}"
if [[ "${branch}" == "prod_${release.version}" ]]; then
    upload "server-external-${release.version}"
fi
