#!/bin/bash

DOCKERFILE="`dirname \"$0\"`/Dockerfile"
DOCKER_WS=`pwd`
# This is helper for building docker image from debian package for mediaserver
# Possible arguments
# -d|--deb ... deb Uses deb file as a source
# -b|--build ... Uses build folder as a source
# -u|--url URL Uses URL as a source for debian file

# Name of docker container to be built
CONTAINER_NAME=mediaserver

while [[ $# -gt 0 ]] ; do

key="$1"

case $key in
	-d|--deb)
	# Will use existing deb file
	DEB_FILE="$2"
	shift
	shift
	;;
	-b|--build)
	# Will use nx_vms build folder as source
	BUILD_PATH="$2"
	shift
	shift
	;;
	-u|--url)
	# Will use url to deb file as source
	DEB_URL="$2"
	shift
	shift
	;;
	-n|--name)
	# Name for container. By default it is 'mediaserver'
	CONTAINER_NAME="$2"
	shift
	shift
	;;
	*)
	POSITIONAL+=("$1")
	;;
esac

done

set -- "${POSITIONAL[@]}"

alias errcho='>&2 echo'

raise_error() {
	>&2 echo "$1. Exiting..."
	exit 1
}

# It checks whether deb file exists and suits us
check_dpkg () {
	dpkg -I "$1" 1> /dev/null || raise_error "$1 is not a deb file"
	return 0
}

if [ ! -r $DOCKERFILE ] ; then
	raise_error "$DOCKERFILE does not exists. It should be near to build.sh script"
fi

if [ ! -z "$DEB_FILE" ] ; then
	echo "I will try to use deb file $DEB_FILE"
	check_dpkg "$DEB_FILE"
	cp "$DEB_FILE" "$DOCKER_WS/"
	DEB_NAME=$(basename $DEB_FILE)
else
	if [ ! -z "$DEB_URL" ] ; then
		echo "I will try to download mediaserver deb from $DEB_URL"
		curl -o mediaserver.deb $DEB_URL 1> /dev/null || raise_error "Failed to download from $DEB_URL"
		DEB_NAME=mediaserver.deb
		check_dpkg "$DEB_NAME"
	else
		# Trying to use build folder for nx_vms and take deb from there
		# TODO: implement it properly
		# TODO: Enable distribution build
		# TODO: Clean up previous build if necessary
		# TODO: Find output name for debian file
		ninja -C "${BUILD_PATH}" distribution_deb_mediaserver
	fi
fi

echo "Building container at folder $DOCKER_WS using mediaserver_deb=$DEB_NAME name=$CONTAINER_NAME"
docker build -t $CONTAINER_NAME --build-arg mediaserver_deb="$DEB_NAME" - < $DOCKERFILE
