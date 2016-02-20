#!/bin/sh

SOURCE=${NX_RSYNC_SOURCE:-'enk.me'}

mkdir -p ${environment.dir}/${qt.path}
rsync -rtvl --delete rsync://$SOURCE/buildenv/${qt.path} ${environment.dir}/${qt.path}/..
