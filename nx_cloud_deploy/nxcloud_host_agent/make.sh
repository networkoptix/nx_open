#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=nxcloud_host_agent
BUILD_ARGS=(--build-arg NXCLOUD_VERSION=${NXCLOUD_VERSION} --build-arg VERSION=${VERSION} --build-arg MODULES="${MODULES[@]}")

function stage()
{
    rm -rf stage

    echo Downloading docker, docker-compose and docker-machine if required...
    (cd docker; wget -q -N https://s3.amazonaws.com/nxcloud-devtools/linux/docker-machine/0.9.0vig/docker-machine; chmod 755 docker-machine) &
    (cd docker; wget -q -N https://s3.amazonaws.com/nxcloud-devtools/linux/docker/1.13.1/docker; chmod 755 docker) &
    (cd docker; wget -q -N https://s3.amazonaws.com/nxcloud-devtools/linux/docker-compose/1.11.2/docker-compose; chmod 755 docker-compose) &

    wait
    echo done
}

main $@
