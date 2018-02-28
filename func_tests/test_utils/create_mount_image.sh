#!/bin/bash -xe

SIZE=12  # GB
IMAGE=/disk2.image
MOUNT_POINT=/mnt/disk2

if [ ! -f "$IMAGE" ]; then
	dd if=/dev/zero of="$IMAGE" bs=$((1024 * 1024)) count=$((1024 * $SIZE))
	/sbin/mke2fs -F -t ext3 "$IMAGE"
fi

if ! grep "$MOUNT_POINT" /proc/mounts; then
	sudo mkdir -p "$MOUNT_POINT"
	sudo mount "$IMAGE" "$MOUNT_POINT"
fi
