#!/bin/bash -ex

. ../environment
. ../common.sh

MODULE=cloud_portal
VERSION=${CLOUD_PORTAL_VERSION:-$VERSION}
BUILD_ARGS=(--build-arg CACHE_DATE=$(date +%s))

function build()
{
    [ -z "$NX_PORTAL_DIR" ] && { echo "NX_PORTAL_DIR variable is unset"; exit 1; }

    cd $NX_PORTAL_DIR/build_scripts
    ./build.sh
}

function stage()
{
    rm -rf stage

	mkdir -p stage/cloud/static/common/static
	rsync -a $NX_PORTAL_DIR/cloud stage
    rm -rf stage/cloud/.idea
}

function stage_cmake()
{
    local cmakeBuildDirectory=$1
    #local moduleName=$2

    rm -rf stage

	mkdir -p stage/cloud/static/common/static
	rsync -a $cmakeBuildDirectory/cloud_portal/cloud_portal/cloud stage
    rm -rf stage/cloud/.idea
}

main $@
