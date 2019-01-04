#!/bin/bash -e

MODULE=mediaserver
VERSION=4.0.0.28077

function build ()
{
    rm -rf build
    mkdir build

    #Set a variable with the script location
    scriptDir=$(dirname -- "$(readlink -f -- "$BASH_SOURCE")")
    #Change to build directory, Extract the .deb file.
    #tar J:filter the archive through xz; xf:Extract all files from archive.tar
    (cd build; ar x $scriptDir/*$VERSION*.deb; tar xJf data.tar.xz)
}

function stage ()
{
    rm -rf stage

	mkdir -p stage/mediaserver stage/var/log
	#Recursive and hardlink instead of copy. f is force
    cp -Rlf build/opt/networkoptix/mediaserver/bin/{mediaserver-bin,mediaserver}
    cp -Rl build/opt/networkoptix/mediaserver/{bin,lib,plugins} stage/mediaserver

}

function pack()
{
    echo "Packing $MODULE:$VERSION to a container"
    #build the image
    docker build -t dockermediaserver .
}

function main()
{
    local args=""
    local n=1

    # Check if we have docker here
    if ! docker info &> /dev/null; then
        echo 'No docker server found. Make sure you have docker installed'
        exit 1
    fi

    numargs=$#
    for ((n=1;n <= numargs; n++))
    do
        func=$1; shift
        args=""

        if [ "$func" = "publish" ]
        then
            args="$1"; shift; n=$((n+1))
        fi
        if [ "$func" = "stage_cmake" ]
        then
            args="$1 $2"; shift; n=$((n+2))
        fi

        $func $args
    echo $args
    echo ______________________________________
    done
}
main $@