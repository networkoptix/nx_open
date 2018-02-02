#!/bin/bash

set -e

SOURCE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BRANCH_FILE="$SOURCE_DIR"/../.hg/branch
RDEP="$SOURCE_DIR"/../build_utils/python/rdep.py
WEBADMIN_FILE="$PWD"/server-external/bin/external.dat
PACKAGES_DIR="$environment"/packages
PACKAGE_BASE_NAME=server-external

function deploy_package()
{
    PACKAGE_DIR=$1

    echo "Updating $PACKAGE_DIR"

    [ -d "$PACKAGE_DIR" ] && rm -rf "$PACKAGE_DIR"
    mkdir -p "$PACKAGE_DIR"/bin

    cp $WEBADMIN_FILE "$PACKAGE_DIR"/bin
    pushd $PACKAGE_DIR > /dev/null
    $RDEP -uf
    popd > /dev/null
}

function check_file()
{
    if [ ! -e "$1" ]
    then
        if [ -z "$2" ]
        then
            echo "$1: File not found." >&2
        else
            echo $2
        fi

        exit 1
    fi
}

check_file "$BRANCH_FILE"
check_file "$RDEP"
check_file "$WEBADMIN_FILE" \
    "$WEBADMIN_FILE is not found. Build webadmin project before deploying it."
check_file "$PACKAGES_DIR"/.rdep \
    "RDep repository is not found in $PACKAGES_DIR."

BRANCH=$(cat $BRANCH_FILE)

deploy_package "$PACKAGES_DIR/any/${PACKAGE_BASE_NAME}-${BRANCH}"
if [[ $BRANCH =~ [0-9]\.[0-9]_web$ ]]
then
    deploy_package "$PACKAGES_DIR/any/${PACKAGE_BASE_NAME}-${BRANCH%_web}"
fi
