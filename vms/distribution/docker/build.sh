#!/bin/bash

# This is helper script for building docker image from debian package for mediaserver

display_usage() {
	echo "I am helper script for building docker image from debian package for mediaserver."
	echo -e "Usage:\nbuild.sh [-u url ] | [-d deb] | path\n"
	echo "Possible arguments:"
	echo -e "\t-d|--deb PATH - uses deb file as a source"
	echo -e "\t-b|--build PATH - uses build folder as a source"
	echo -e "\t-u|--url URL - uses URL as a source for debian file"
}

# Path to a folder with Dockerfile and necessary scripts
DOCKER_SOURCE="`dirname \"$0\"`"

DOCKERFILE="$DOCKER_SOURCE/Dockerfile"

# Path to current docker build folder
DOCKER_WS=`pwd`

# Name of docker container to be built
CONTAINER_NAME=mediaserver
CLOUD_OVERRIDE=
POSITIONAL=()


# Processing command line arguments
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
	-c|--cloud)
		CLOUD_OVERRIDE="$2"
		shift
		shift
		;;
		*)
		POSITIONAL+=("$1")
		shift
		;;
	esac
done

# Trying to guess what to do with positional arguments
for path in "${POSITIONAL[@]}" ; do
	if [[ $path == http://* ]] || [[ $path == https://* ]] ; then
		#echo "The path ${SS}$path${EE} looks like URL. I will try to download it"
		DEB_URL=$path
	else
		#echo "The path ${SS}$path${EE} looks like regular file. I will copy it to current directory"
		DEB_FILE=$path
	fi
done

raise_error() {
	>&2 echo -e "$1. Exiting..."
	exit 1
}

# Style options to display paths and important variables.
SS="\e[4m"
EE="\e[0m"

# It checks whether deb file exists and suits us.
check_dpkg ()
{
	local file=$1

	if [[ ! -r $file ]]; then
		raise_error "File ${SS}$file${EE} does not exist or I can not access it"
	fi
	# Trying to check whether this file is a proper DEB package
	if hash dpkg 2>/dev/null; then
		dpkg -I "$file" 1> /dev/null || raise_error "${SS}${file}${EE} is not a deb file"
	else
		# Some hosts do not have 'dpkg', so we check extension only.
		[[ $file == *.deb ]] || raise_error "${SS}${file}${EE} does not look like deb package"
	fi
	return 0
}

if [ ! -r $DOCKERFILE ] ; then
	raise_error "${SS}${DOCKERFILE}${EE} does not exists. It should be near to build.sh script"
fi


if [ ! -z "$DEB_FILE" ] ; then
	echo -e "I will try to use deb file ${SS}${DEB_FILE}${EE}"
	check_dpkg "$DEB_FILE"
	cp "$DEB_FILE" "$DOCKER_WS/"
	DEB_NAME=$(basename $DEB_FILE)
else
	if [ ! -z "$DEB_URL" ] ; then
		echo -e "I will try to download mediaserver deb from ${SS}${DEB_URL}${EE}"
		curl -o mediaserver.deb $DEB_URL 1> /dev/null || raise_error "Failed to download from ${SS}$DEB_URL${EE}"
		DEB_NAME=mediaserver.deb
		check_dpkg "$DEB_NAME"
	else
		>&2 echo "Neither URL nor direct path are specified"
		display_usage
		exit 1
		# Trying to use build folder for nx_vms and take deb from there
		# TODO: implement it properly
		# TODO: Enable distribution build
		# TODO: Clean up previous build if necessary
		# TODO: Find output name for debian file
		#ninja -C "${BUILD_PATH}" distribution_deb_mediaserver
	fi
fi

echo -e "Building container at ${SS}${DOCKER_WS}${EE} using mediaserver_deb=$DEB_NAME name=$CONTAINER_NAME"
cp "$DOCKER_SOURCE/manage.sh" "$DOCKER_WS"
docker build -t $CONTAINER_NAME --build-arg mediaserver_deb="$DEB_NAME" --build-arg cloud_host="$CLOUD_OVERRIDE" -f - . < $DOCKERFILE
