#!/bin/bash -e
. ../environment
. ../common.sh

MODULE=connection_mediator

function stage_cmake_extra()
{                                                                                    	
    mkdir -p stage/${moduleName}/var
	echo "TRYING cp -l $environment/packages/any/geolite-2/GeoLite2-City.mmdb stage/${moduleName}/var/"
    cp -l $environment/packages/any/geolite-2/GeoLite2-City.mmdb stage/${moduleName}/var/
}

function stage()
{
	stage_cpp
}


main $@
