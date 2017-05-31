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
	rsync -a --exclude='static/*/static/fonts' $NX_PORTAL_DIR/cloud stage
    rsync -a $NX_PORTAL_DIR/cloud/static/default/static/fonts stage/cloud/static/common/static
    rm -rf stage/cloud/.idea
}

main $@
