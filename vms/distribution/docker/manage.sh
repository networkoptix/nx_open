#!/bin/bash

# This is helper script to be installed inside container.
# It can:
#  - wrap some actions from Dockerfile
#  - change cloud host for mediaserver.

display_usage() {
	echo "This is helper for building docker image from debian package for mediaserver."
	echo -e "Usage:\nmanage.sh [--cloud cloud_host] [--norestart]\n"
	echo "Possible arguments:"
	echo -e "\t-c|--cloud cloud_host patches mediaserver to use specified cloud host"
	echo -e "\t--norestart pending operations will not restart mediaserver through systemd"
}

set -eu

MEDIASERVER_CONF=/opt/networkoptix/mediaserver/etc/mediaserver.conf
MEDIASERVER_NETWORK_SO=/opt/networkoptix/mediaserver/lib/libnx_network.so

CLOUD_HOST_NAME_WITH_KEY=$(eval echo \
"this_is_cloud_host_name cloud-test.hdw.mx\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")

# Should we stop/start systemd networkoptix-mediaserver service while changing cloud host
NORESTART=0

SYSTEMD_SERVICE="networkoptix-mediaserver"

raise_error() {
	>&2 echo -e "$1. Exiting..."
	exit 1
}

# Deals with setting up local systemd mounts
# It is not used right now
setup_systemd()
{
	if nsenter --mount=/host/proc/1/ns/mnt -- mount | grep /sys/fs/cgroup/systemd >/dev/null 2>&1; then
	  echo 'The systemd cgroup hierarchy is already mounted at /sys/fs/cgroup/systemd.'
	else
	  if [ -d /host/sys/fs/cgroup/systemd ]; then
	    echo 'The mount point for the systemd cgroup hierarchy already exists at /sys/fs/cgroup/systemd.'
	  else
	    echo 'Creating the mount point for the systemd cgroup hierarchy at /sys/fs/cgroup/systemd.'
	    mkdir -p /host/sys/fs/cgroup/systemd
	  fi

	  echo 'Mounting the systemd cgroup hierarchy.'
	  nsenter --mount=/host/proc/1/ns/mnt -- mount -t cgroup cgroup -o none,name=systemd /sys/fs/cgroup/systemd
	fi
	echo 'Your Docker host is now configured for running systemd containers!'
}

# If no CLOUD_HOST_KEY, show the current text, otherwise, patch FILE to set the new text.
# [in] NEW_CLOUD_HOST
# Extracted from devtools/utils/patch-cloud-host.sh
patch_cloud_host()
{
	local FILE=${MEDIASERVER_NETWORK_SO}
	local CLOUD_HOST_KEY="this_is_cloud_host_name"
	local FILE_PATH_REGEX=".*/\(libnx_network\.\(a\|so\)\|nx_network\.dll\)"
    local STRING=$(strings --radix=d -d "$FILE" |grep "$CLOUD_HOST_KEY")
    [ -z "$STRING" ] && raise_error "'$CLOUD_HOST_KEY' string not found in $FILE"

    local OFFSET=$(echo "$STRING" |awk '{print $1}')
    local EXISTING_CLOUD_HOST=$(echo "$STRING" |awk '{print $3}')

    if [ -z "$NEW_CLOUD_HOST" ]; then
        #raise_error "Cloud Host is '$EXISTING_CLOUD_HOST' in $FILE"
        return 0
    elif [ "$NEW_CLOUD_HOST" = "$EXISTING_CLOUD_HOST" ]; then
        echo "Cloud Host is already '$EXISTING_CLOUD_HOST' in $FILE"
        return 0;
    else
    	if [ "$NORESTART" == 0 ]; then
    		systemctl stop $SYSTEMD_SERVICE || true
    	fi

        local PATCH_OFFSET=$(expr "$OFFSET" + ${#CLOUD_HOST_KEY} + 1)
        local NUL_OFFSET=$(expr "$PATCH_OFFSET" + ${#NEW_CLOUD_HOST})
        local NUL_LEN=$(expr \
            ${#CLOUD_HOST_NAME_WITH_KEY} - ${#CLOUD_HOST_KEY} - 1 - ${#NEW_CLOUD_HOST})
        echo "CLOUD_HOST_NAME_WITH_KEY len: ${#CLOUD_HOST_NAME_WITH_KEY}"
        echo "CLOUD_HOST_KEY len: ${#CLOUD_HOST_KEY}"

        # Patching the printable chars.
        echo -ne "$NEW_CLOUD_HOST" |dd bs=1 conv=notrunc seek="$PATCH_OFFSET" of="$FILE" \
            2>/dev/null || raise_error "failed: dd of text"

        # Filling the remaining bytes with NUL chars.
        dd bs=1 conv=notrunc seek="$NUL_OFFSET" of="$FILE" if=/dev/zero count="$NUL_LEN" \
            2>/dev/null || raise_error "failed: dd of NULs"

        echo "Cloud Host was '$EXISTING_CLOUD_HOST', now is '$NEW_CLOUD_HOST' in $FILE"

        if [ "$NORESTART" == 0 ]; then
    		systemctl start $SYSTEMD_SERVICE || true
    	fi
    fi
}


POSITIONAL=()

# Doing some arguments parsing
while [[ $# -gt 0 ]] ; do
	key="$1"

	case $key in
	-c|--cloud)
		NEW_CLOUD_HOST="$2"
		shift
		shift
		;;
	--norestart)
		NORESTART=1
		shift
		;;
	*)
		POSITIONAL+=("$1")
		shift
		;;
	esac
done

if [ -n "$NEW_CLOUD_HOST" ] ; then
	patch_cloud_host
else
	echo "Leaving cloud_host as is"
fi