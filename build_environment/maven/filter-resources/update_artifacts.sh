#!/bin/sh

SOURCE=${NX_RSYNC_SOURCE:-'enk.me'}

mkdir -p ${qt.path}
rsync -rtvL --delete rsync://$SOURCE/buildenv/${qt.path} ${qt.path}/..
