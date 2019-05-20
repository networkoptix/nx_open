#!/bin/bash -e
. ../environment
. ../common.sh

MODULE=connection_mediator

function copy_geolite_db()
{
    mkdir -p stage/${moduleName}/var
    cp -rl $environment/packages/any/geolite-2/GeoLite2-City.mmdb stage/${moduleName}/var/
}

function stage()
{
    stage_cpp
    copy_geolite_db
}

main $@
