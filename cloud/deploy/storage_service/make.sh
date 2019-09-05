#!/bin/bash

. ../environment
. ../common.sh

MODULE=storage_service

function stage_cmake_extra()
{
    mkdir -p stage/${moduleName}/var
    cp -l $environment/packages/any/geolite-2/GeoLite2-City.mmdb stage/${moduleName}/var/
}

main $@
