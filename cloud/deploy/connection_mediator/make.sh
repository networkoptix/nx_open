#!/bin/bash -e
. ../environment
. ../common.sh

MODULE=connection_mediator

function stage()
{
    stage_cpp
    
    mkdir -p stage/${moduleName}/var
    cp -rl $environment/packages/any/geolite-2/GeoLite2-City.mmdb stage/${moduleName}/var/
}

main $@
