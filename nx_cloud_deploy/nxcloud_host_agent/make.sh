#!/bin/bash -e

. ../environment
. ../common.sh

if [ "${NXCLOUD_VERSION}" = latest ]
then
    NXCLOUD_VERSION=$(pip search --trusted-host=la.hdw.mx --index=http://la.hdw.mx:8006 nxcloud | awk 'BEGIN {n=1} { if (n == 1) print $NF; n++ }')
fi

MODULE=nxcloud_host_agent
BUILD_ARGS=(--build-arg NXCLOUD_VERSION=${NXCLOUD_VERSION} --build-arg VERSION=${VERSION} --build-arg MODULES="${MODULES[@]}")

function stage()
{
    rm -rf stage

    echo Downloading docker, docker-compose and docker-machine if required...
    (cd docker; wget -q -N https://s3.amazonaws.com/nxcloud-devtools/linux/docker-machine/0.9.0vig/docker-machine; chmod 755 docker-machine) &
    (cd docker; wget -q -N https://s3.amazonaws.com/nxcloud-devtools/linux/docker/1.13.1/docker; chmod 755 docker) &
    (cd docker; wget -q -N https://s3.amazonaws.com/nxcloud-devtools/linux/docker-compose/1.11.2/docker-compose; chmod 755 docker-compose) &
    (cd docker; wget -q -N https://storage.googleapis.com/kubernetes-release/release/v1.6.4/bin/linux/amd64/kubectl; chmod 755 kubectl) &

    wait
    echo done
}

main $@
