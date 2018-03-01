#!/bin/bash -xe

SIZE=12  # GB
IMAGE=/disk2.image
MOUNT_POINT=/mnt/disk2

if [ ! -f "$IMAGE" ]; then
	fallocate -l $((1024*1024*1024 * $SIZE)) "$IMAGE"
	/sbin/mke2fs -F -t ext4 "$IMAGE"
fi

if ! grep "$MOUNT_POINT" /proc/mounts; then
	sudo mkdir -p "$MOUNT_POINT"
	sudo mount "$IMAGE" "$MOUNT_POINT"
fi
