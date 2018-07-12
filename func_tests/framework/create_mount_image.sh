#!/bin/bash -xe

# This second disk is required for backup_storage_test tests.
# mediaserver uses a disk if it's size is not less than 1/10 of other used storage disks
# (currently 42G / 10 = 4.2G) minus minStorageSpace set in it's config file (by default 10G)
# so if we make 10G disk and set minStorageSpace to 1M, then 10-0.001 > 42/10, and this size will be ok

SIZE=10  # GB
IMAGE=/disk2.image
MOUNT_POINT=/mnt/disk2

if [ ! -f "$IMAGE" ]; then
	sudo fallocate -l $((1024*1024*1024 * $SIZE)) "$IMAGE"
	sudo /sbin/mke2fs -F -t ext4 "$IMAGE"
fi

if ! grep "$MOUNT_POINT" /proc/mounts; then
	sudo mkdir -p "$MOUNT_POINT"
	sudo mount "$IMAGE" "$MOUNT_POINT"
fi
