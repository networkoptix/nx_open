#!/bin/bash -e
. ../environment
. ../common.sh

MODULE=connection_mediator

function stage_cmake_extra()
{                                                                                    	
    mkdir -p stage/${moduleName}/var
	echo "TRYING cp -l $environment/packages/any/geolite-2/GeoLite2-City.mmdb stage/${moduleName}/var/"
    cp -l $environment/packages/any/geolite-2/GeoLite2-City.mmdb stage/${moduleName}/var/
	echo "return code: $?"
	echo "current dir: `pwd`"
	test -f "stage/${moduleName}/var/GeoLite2-City.mmdb" && echo "stage/${moduleName}/var/GeoLite2-City.mmdb exists"
}

function stage()
{
	stage_cpp
}


main $@
