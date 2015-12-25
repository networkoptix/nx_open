#!/bin/sh

SOURCE=${NX_RSYNC_SOURCE:-'enk.me'}

mkdir -p ${qt.path}
rsync -rtvl --delete rsync://$SOURCE/buildenv/${qt.path} ${qt.path}/..
